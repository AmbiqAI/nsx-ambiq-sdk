# Core SDK Scope

The unified `nsx-ambiq-sdk` package publishes a curated NSX core SDK surface
for Ambiq silicon across AmbiqSuite release trains. The current stable provider
line is `nsx-ambiqsuite-r5`. It does not republish the full AmbiqSuite zip.

AmbiqSuite drops are broad distribution archives: they include HAL, BSP,
headers, startup/system code, prebuilt libraries, examples, middleware,
third-party packages, debug assets, and board-specific application material.
The package should extract only the substrate needed by NSX platform modules.

## In Scope For The Package

The core SDK bundle may include or describe:

- SDK-provider metadata and version anchors;
- SoC and skew descriptors;
- board descriptors for Ambiq-owned reference boards;
- approved AmbiqSuite headers needed to compile HAL/BSP users;
- HAL and BSP prebuilt libraries when redistributable;
- startup, system, linker, and vector integration selected by descriptors;
- CMSIS 6 Core(M) headers when carried as an NSX-owned module;
- NSX-owned runtime foundation modules;
- NSX-owned platform services such as power, PMU, performance timers, portable
  APIs, and basic UART/I2C/SPI shims;
- tests that validate NSX descriptors, artifacts, wrappers, and minimal linking.

## Out Of Scope For The Package

These should not be pulled into the core SDK bundle by default:

- AmbiqSuite example applications;
- FreeRTOS source or ports;
- TinyUSB source or ports in the base SDK provider module;
- Cordio/Bluetooth middleware;
- SEGGER tools or vendor debug packages beyond descriptor references;
- USB stacks and class drivers such as `src/usb` in the base SDK provider module;
- CMSIS-NN and CMSIS-DSP libraries;
- large board demo assets, sample applications, or product-specific examples;
- board-device convenience buckets such as legacy button, PSRAM, or NVM helper
  modules;
- third-party middleware not required to build the NSX core substrate;
- raw HAL/BSP source when it is not redistributable.

Out-of-scope components can still be supported as separate optional NSX modules
or application dependencies. When the component is shipped by the AmbiqSuite R5
drop and tightly coupled to the R5 HAL/BSP, it may live in this repository as a
separate opt-in module. It must not be bundled into the base `nsx-ambiqsuite-r5`
provider surface.

## Optional Module Candidates

| Component | Preferred home |
| --- | --- |
| FreeRTOS | `nsx-freertos` or application-owned RTOS module |
| TinyUSB | `nsx-ambiq-usb-r5` for the SDK-vendored R5 substrate; wrapper policy in `nsx-usb` |
| Ambiq USB integration | `nsx-ambiq-usb-r5` substrate plus higher-level `nsx-usb` wrapper |
| CMSIS-NN | separate `nsx-cmsis-nn` module/repo |
| CMSIS-DSP | separate `nsx-cmsis-dsp` module/repo |
| Cordio/Bluetooth | separate wireless/Bluetooth module family |
| SEGGER RTT/SystemView/J-Link helpers | tooling/debug module, not core SDK |
| AmbiqSuite examples | examples repo or app templates, not core SDK |
| GPIO button/trigger helpers | `nsx-gpio-input` or board/application module |
| PSRAM setup helpers | `nsx-external-memory` or `nsx-psram-ambiqsuite-r5` |
| MSPI flash/NVM helpers | `nsx-mspi-flash` plus concrete flash driver module |

## Intake Rule

SDK intake tooling should classify every discovered file or directory as one of:

1. `core-descriptor-input`: used to update CMake descriptors or manifests;
2. `core-artifact`: eligible to bundle after release approval;
3. `optional-module-candidate`: useful, but outside this core repo;
4. `ignored-upstream-content`: examples, demos, raw source, or unrelated assets.

Anything not explicitly classified as core should be left in the local raw SDK
staging area.

## Why This Boundary Matters

The core bundle should be small enough to version and validate as a platform
substrate. Optional middleware such as FreeRTOS and TinyUSB brings separate
configuration, licensing, tests, and release cadence. Keeping those outside the
core SDK prevents a new AmbiqSuite zip from expanding NSX's baseline dependency
surface every time SWS adds examples or middleware.

## Practical Migration Path

- Prefer NSX modules as the default application-facing API surface for common flows.
- Leave AmbiqSuite available underneath for lower-level control and gaps that NSX does not wrap yet.
- Wire up the common NSX runtime modules first, then wrap repeated app patterns over time.
- Reduce legacy helper usage when it hard-codes board or app policy, but do not rewrite working code just to remove old names.
- Keep compatibility helpers when they are still useful and not actively harmful; tighten or relocate them later as the preferred NSX path gets stronger.
