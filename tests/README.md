# Test Strategy

Tests in this repo validate NSX's adapter layer around AmbiqSuite. They should
stay focused on what NSX changes or selects: descriptors, CMake targets,
capabilities, paths, prebuilt artifacts, wrappers, and minimal integration.

SWS AmbiqSuite tests remain the source of truth for HAL and BSP implementation
correctness.

`test_cpp_startup.py` links the shipped apollo2/apollo3/apollo3p startup and
linker scripts with a C++ global object with a nontrivial destructor, checks the reset-handler call,
and verifies a nonempty constructor table in flash, numeric constructor
priority order, resolved runtime array bounds, and a binary below 2 MiB.
GCC runs when `arm-none-eabi-gcc` is under `GCC_ROOT/bin` or on PATH (required
in the cross-compile CI job); ATfE runs when installed under
`/opt/toolchains/atfe` or `ATFE_ROOT`.
Missing toolchains skip locally. The fixture exposes `constructor_result` for
an optional debugger check: `0x600DC0DE` means the constructor ran after memory
initialization, with preinit hooks before constructors in priority order, and
the global destructor and fini-array callback ran through explicit `exit(0)`. The fixture's `_exit`
hook records the final result. Reset still enters a breakpoint if `main`
returns; automatic process exit is not added. Both startup defaults and
application-provided strong CRT definitions are linked to guard override support.
Build checks alone do not establish execution on hardware. The scripts define
both the traditional newlib array bounds and the combined preinit/init bounds
used by ATfE 22.1 picolibc.
They also enable newlib's lazy fini-array registration, so `exit` support
registers finalizers before user constructors without pulling exit into apps
that never use it.
The GCC fixture uses the toolchain's default full newlib. Custom
`--specs=nano.specs` CRT/exit integration is outside this fixture's scope:
nano needs its CRT to register finalizers and link the required exit handlers.

Published release upgrade-file lists are checked against their immutable tag;
before the tag exists they are checked against HEAD. This keeps later fixes
out of an older release's qualification claims. Provider and archive integrity
checks continue to inspect the current payload.

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
