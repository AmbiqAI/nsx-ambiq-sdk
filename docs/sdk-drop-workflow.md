# SDK Drop Workflow

This repo is the NSX adapter and validation layer for AmbiqSuite R5-family SDK
drops. It should make new SWS drops repeatable without turning the repo into a
hand-maintained fork of AmbiqSuite.

## Goals

- Accept a new SWS AmbiqSuite drop, such as R5.4.0, in a local staging path.
- Discover or pseudo-discover HAL, BSP, board, SoC, startup, system, and prebuilt
  library layout changes.
- Update NSX SoC and board descriptors with the smallest possible adapter delta.
- Bundle only redistributable artifacts for HAL/BSP when source cannot be
  released.
- Exclude AmbiqSuite examples and broad middleware from the core bundle unless a
  deliberate optional NSX module is created for them.
- Keep CMSIS-Core 6 in the core SDK substrate, while treating CMSIS-NN and
  CMSIS-DSP as separate optional module/repo inputs.
- Keep higher-level NSX modules as source when they are owned by NSX and are
  legally releasable.
- Validate that NSX's adapter code and overlays are sound without duplicating the
  full AmbiqSuite test burden.

## Source And Artifact Boundaries

SWS owns AmbiqSuite source validation. NSX owns the integration boundary.

| Area | Source of Truth | NSX repo behavior |
| --- | --- | --- |
| Raw HAL/BSP source | SWS AmbiqSuite drop | local staging input only; do not commit unless explicitly cleared |
| HAL/BSP prebuilt libraries | SWS or NSX-internal build from raw drop | may be bundled when redistributable |
| AmbiqSuite headers | SWS AmbiqSuite drop | bundle only according to license/release policy |
| AmbiqSuite examples | SWS AmbiqSuite drop | do not bundle in the core SDK repo |
| FreeRTOS, TinyUSB, Cordio, USB stacks | SWS/third party | separate optional modules or app dependencies |
| CMSIS-NN and CMSIS-DSP | ARM CMSIS projects / NSX optional modules | separate repos/modules, not bundled in core SDK |
| Startup/linker/system selection | NSX adapter descriptors | commit descriptors and minimal overlays |
| Board/SoC capability model | NSX adapter descriptors | commit as CMake source of truth |
| Runtime/services/I/O modules | NSX | commit source and tests |

## Recommended Layout

Use local staging for raw SDK drops and a separate bundled artifact surface for
redistributable payloads:

```text
sdk-intake/
  build_ambiqsuite.py      # source-controlled intake/build helper
  local/                   # local only, ignored by sdk-intake/local/.gitignore
    drops/
      AmbiqSuite_R5.4.0.zip
    work/
      AmbiqSuite_R5.4.0/

artifacts/
    ambiqsuite/R5.4.0/
        include/             # redistributable headers if approved
    <toolchain>/lib/<part>/libam_hal.a
    <toolchain>/lib/<part>/<bsp>/libam_bsp.a
        manifest.yaml

cmake/sdks/
    ambiqsuite-r5.4.0.cmake  # SDK-provider descriptor

cmake/socs/
    apollo510.cmake
    apollo510b.cmake
    ...

boards/
    apollo510_evb/board.cmake
    ...
```

The exact artifact path can change, but the important split should not: raw SDK
source is an input, bundled binaries/approved headers are outputs, and NSX CMake
descriptors are the adapter.

## Intake Steps

1. Place the SWS zip under `sdk-intake/local/drops/AmbiqSuite_R<version>.zip`.
2. Scan the native layout:
   - `mcu/<part-or-family>/hal/...`
   - `src/<part-or-family>/system_*.c`
   - `boards/<ambiq-board>/bsp/...`
   - `lib/<part>/libam_hal.*`
   - `lib/<part>/<board-lib-subdir>/libam_bsp.*`
3. Classify discovered content as core descriptor input, approved core artifact,
  optional module candidate, or ignored upstream content. Examples, FreeRTOS,
  TinyUSB, Cordio, USB stacks, CMSIS-NN, CMSIS-DSP, and raw HAL/BSP source
  should not enter the core bundle by default.
4. Compare discovered parts and boards against existing `cmake/socs/` and
   `boards/` descriptors.
5. Add or update SDK-provider and SoC/skew descriptors. Prefer variables that
   map native AmbiqSuite names rather than copying vendor source.
6. Add or update board descriptors only for Ambiq-owned reference boards that
   this repo should expose.
7. Build or import approved HAL/BSP prebuilt libraries for the release payload.
8. Run NSX validation tests.
9. Update module metadata and the NSX registry so logical modules resolve to the
   same physical repo revision.

## Build Helper

The source-controlled helper in `sdk-intake/` automates the HAL/BSP build and
artifact copy steps for R5.2.0 while keeping raw SDK source in ignored staging.

```bash
python sdk-intake/build_ambiqsuite.py --version R5.2.0 --toolchain gcc
python sdk-intake/build_ambiqsuite.py --version R5.2.0 --toolchain atfe --atfe-root /path/to/ATfE-22.1.0
python sdk-intake/build_ambiqsuite.py --version R5.2.0 --toolchain acfe --acfe-root /path/to/ACfE
```

The dev/CI container installs GCC, ATfE, and ACfE under `/opt/toolchains`. The
`Container Validation` workflow has an opt-in `run_sdk_build` mode that mounts a
runner-local AmbiqSuite zip read-only and runs the same helper inside the
container:

```text
run_sdk_build: true
sdk_toolchain: gcc | atfe | acfe | all
ambiqsuite_zip_path: /ambiqsuite/AmbiqSuite_R5.2.0.zip
```

ACfE builds require `ARMLM_ACTIVATION_CODE` to be configured as a repository
secret so `armlm` can activate inside the container before invoking the build
helper. The workflow runs ACfE licensing as container `root` and restores
generated artifact ownership after the build.

`--toolchain` may be repeated, or set to `all`. Single-toolchain reruns rebuild
only that native output directory, copy only that toolchain's libraries into
`artifacts/ambiqsuite/<version>/`, and regenerate the manifest from the artifact
files currently present. This lets ACfE be run later on a licensed machine using
the same zip and script without rebuilding GCC or ATfE.

The helper builds release-style artifacts by default. It appends `-g0` after the
native AmbiqSuite makefile flags to override their default `-g`, keeping HAL/BSP
archives from accumulating debug sections. Use `--debug-symbols` only for local
diagnostic builds.

Promotion is repeatable and destructive for the provider payload:

```bash
python sdk-intake/build_ambiqsuite.py --version R5.2.0 --promote-only
```

The promoted `modules/nsx-ambiqsuite-r5/sdk/` tree contains approved headers,
system sources, the three utility sources linked by `nsx-ambiq-hal-r5`, and
prebuilt HAL/BSP libraries under `sdk/lib/<toolchain>/...`. It intentionally does
not retain raw HAL/BSP source, examples, resource source files, or MSPI device
driver source files.

## Pseudo-Automation Targets

The intake can be semi-automated before it is fully automated. Useful checks:

- list new or removed `mcu/*` directories;
- list new or removed `boards/*/bsp` directories;
- list new or removed `lib/*/libam_hal.*` and `lib/*/*/libam_bsp.*` files;
- list out-of-scope directories such as examples, FreeRTOS, TinyUSB, Cordio,
  SEGGER tooling, USB stacks, CMSIS-NN, and CMSIS-DSP without importing them
  into core;
- infer candidate `NSX_AMBIQ_BOARD_NAME` and `NSX_AMBIQ_BSP_LIB_SUBDIR` pairs;
- flag SoC skews that share an MCU source directory but have unique linker
  scripts or part defines;
- generate a review manifest for humans to approve before CMake files change.

## Test Ownership

NSX tests should prove the adapter and overlays are correct. They should not try
to replace SWS AmbiqSuite validation.

Recommended test tiers:

| Tier | Purpose |
| --- | --- |
| Descriptor tests | CMake configure tests for every supported SoC/skew and board combination |
| Artifact tests | verify HAL/BSP library paths, headers, startup sources, system sources, and linker scripts exist |
| Capability tests | verify M4/M55, DSP, MVE, FPU, PMU, and memory facts match expected descriptors |
| Module configure tests | configure each logical module with representative boards/toolchains |
| Minimal link tests | build tiny firmware images that link startup, HAL, BSP, core, and selected services |
| Hardware smoke tests | optional board farm or manual flashing for reset, UART, timer, sleep, PMU, and basic I/O |

For HAL/BSP internals, rely on SWS tests and release notes. For NSX differences,
exercise the descriptors, generated build graph, prebuilt ABI selection, and any
source overlays or wrappers we own.

## Release Principle

A new AmbiqSuite drop should require changing data-like descriptors and approved
artifacts first. Hand edits to runtime modules should be treated as a signal that
we found a real adapter gap or an intentional NSX behavior change.
