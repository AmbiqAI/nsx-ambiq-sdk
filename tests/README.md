# Test Strategy

Tests in this repo validate NSX's adapter layer around AmbiqSuite. They should
stay focused on what NSX changes or selects: descriptors, CMake targets,
capabilities, paths, prebuilt artifacts, wrappers, and minimal integration.

SWS AmbiqSuite tests remain the source of truth for HAL and BSP implementation
correctness.

## Running Tests

```sh
python -m pytest
```

The first test layer is intentionally static and local: it parses NSX module
manifests, checks repo shape, verifies the SDK-drop manifest template, and
guards against accidentally importing optional AmbiqSuite middleware into the
core bundle.

## Owned By NSX

- CMake descriptor configure tests for every supported SoC/skew and board.
- Path validation for HAL/BSP libraries, headers, startup files, system files,
  and linker scripts.
- Capability checks for core class, DSP, MVE, FPU, PMU tier, and memory facts.
- Module-level configure tests for logical NSX modules.
- Minimal link tests for startup + HAL + BSP + core/service combinations.
- Wrapper/overlay unit tests for source that lives in this repo.

## Owned By SWS AmbiqSuite

- HAL driver correctness.
- BSP implementation correctness for Ambiq reference boards.
- Low-level register definitions and silicon bring-up behavior.
- Vendor SDK release qualification.

## Practical First Tests

1. Configure every board descriptor with file assertions enabled against a staged
   SDK drop.
2. Configure every logical module against one representative Apollo5 board.
3. Build a tiny startup/HAL/BSP/core firmware image for GCC and ATfE when
   toolchains are available.
4. Validate capability facts from CMake cache output or generated manifest.
5. Run optional hardware smoke tests for UART, timer, sleep, reset, and PMU.
