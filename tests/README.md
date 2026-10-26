# Test Strategy

Tests in this repo validate NSX's adapter layer around AmbiqSuite. They should
stay focused on what NSX changes or selects: descriptors, CMake targets,
capabilities, paths, prebuilt artifacts, wrappers, and minimal integration.

SWS AmbiqSuite tests remain the source of truth for HAL and BSP implementation
correctness.

`test_small_m55_memory.py` rejects nonexistent ITCM profiles on Apollo330P and
Apollo510L, checks GNU/ArmClang memory declarations against the documented part
limits, and uses GCC/ATfE to link the shipped GNU startup with both SBL and NBL
scripts. Separate oversized objects must fail for application NVM, DTCM and
shared SRAM. GCC is required in the Arm cross-compile job; ATfE runs when
`ATFE_ROOT` is available. See `docs/small-m55-memory.md` for source revisions
and the radio-firmware reservation.
If `armlink` is available, additional scatter-file link probes check aggregate
load-image limits as well as execution-region bounds. GNU regressions also
cover shared-data subsections, resource-table copy bounds, empty startup-copy
ranges, and unused C++ code with unwind tables.
`test_power_core_clock.py` exercises the shared Apollo510/Apollo330P/510L
performance selector with a controllable HAL, including variable HP2 PLL
and Apollo510 HFRC2 audio rates, FPGA scaling, and failure paths. It also compiles that source against real HAL headers
using each board's generated CMake flags. GCC is required in the cross-compile
job; ATfE is enabled by `ATFE_ROOT`, and ArmClang by an available `armclang`.

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

`test_system_status.py` injects HAL failures through actual core family backends,
checking first-error propagation, DCU cleanup, incoming interrupt masks, and
invalid requests before startup ownership. Its real-header builds and opt-in
`tests/hardware/test_system_status_smoke.py` smoke cover full/minimal startup with ITM
on AP3P, AP4 KXR, AP510B and AP330P. The smoke checks trace registers and reports
through RTT; it does not establish externally received SWO output or exercise
vendor nonreturning error paths.

## Running Tests

```sh
python -m pytest
```

The first test layer is intentionally static and local: it parses NSX module
manifests, checks repo shape, verifies the SDK-drop manifest template, and
guards against accidentally importing optional AmbiqSuite middleware into the
core bundle.

The intake-patch byte checks reconstruct each patched provider file from the
release record's starting commits, apply the stable patch queue, and compare
the complete Git content with the working tree (honoring Git's line-ending
filters for CRLF checkouts). This catches edits outside patch
hunks as well as missing fixes. Fetch full history and release tags to run
these checks locally; missing history skips locally and fails in CI.

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

`test_vendor_abi.py` compiles standalone SDK consumers using real PDM headers
in C/C++ and extracts the selected vendor implementation into a relocatable
link. GCC is required in cross-compile CI; ATfE/ArmClang cases run when installed.

`test_link_smoke.py` builds complete firmware for four staged M55 boards and
validates entry/vector/stack/vendor symbols and ELF physical load bounds. It
rejects missing startup and corrupted firmware evidence. GCC is required by
the cross-compile job; unavailable optional compilers skip explicitly.

Firmware mutations also remove initialized-data loads, truncate an executable
load inside a required function, shift section file mappings, and move MSP to
the stack limit or below its expected top. All 12 board/compiler images remain
positive controls for GNU copy-down and ArmClang scatter loading.

`test_scatter_load_bounds.py` exercises all 18 scatter profiles with real
`armlink`: valid small/exact-boundary images and RO/aggregate-load overflow.
The linker is optional for ordinary host CI but mandatory for release
qualification of ArmClang layouts; skipped cases do not establish that result.

`test_shared_startup.py` checks all 18 GNU shared-section layouts with GCC and
optional ATfE using resource-only, shared-only, subsection, combined and empty
inputs. With QEMU/GDB installed it executes each family's production data/shared/ITCM-copy
assembly at scratch addresses for empty and nonempty ranges; these are copy-logic
checks, not Apollo hardware or physical-memory-map qualification.

`test_audio_lifecycle.py` fault-injects actual Apollo3/Apollo5 backend calls under
ASan/UBSan. It verifies failure returns, IRQ/state publication, instance ownership,
teardown retry without powered-off register access and observable ISR failures.
Mocks qualify software control flow; physical DMA and capture need board testing.

Apollo5 audio lifecycle tests model active two-stage DMA: a second start fails,
completion service selects alternating buffers, and threshold-only interrupts
must be serviced without producing a callback. They also exercise service
failure, invalid completed-buffer addresses, and capture replacement from a
callback, including the replacement's next frame. Actual AP510B runs are
recorded separately from the host HAL model.

## Opt-in board execution

The local pytest/Unity startup PoC is documented in
[`docs/local-hardware-tests.md`](../docs/local-hardware-tests.md). Ordinary test
runs never access hardware. Explicit build-only runs do not count as hardware
passes. Shared-lab allocation remains a prerequisite to physical execution.

## SDK update acceptance

`uv run --group ci python -m tools.sdk_compatibility --profile gcc --output artifacts/compatibility/RUN_NAME`
requires the payload checks, reviewed GCC header/ABI/link matrix, core firmware
builds and audio/clock adapter fault models to
complete without skips. Use a fresh output directory for each invocation.
The `payload` profile omits the target cross-compilation matrix. See the local hardware guide
above for matrix boundaries and evidence interpretation. Neither profile
accesses boards or claims physical qualification.

The shared hardware API and fixture boundaries are described in the local
hardware guide. `test_hardware_architecture.py` exercises a second synthetic
suite through the shared build and simulated transport; it does not flash a board.

The required GCC intake gate includes AP3 Blue Plus and AP4 KBR/KXR startup
images with retained real HAL/BSP implementations, six runtime firmware builds,
and Apollo2/3/3P constructor/CRT variants. These reuse the shared hardware build
API without opening probes. Host audio lifecycle coverage collects only applicable
backend operations; skipped applicable cases still make qualification incomplete.
AP3 acoustic testing remains paused and is not an intake-gate prerequisite.

## I2C wrapper regressions

`python -m pytest tests/test_i2c_api.py` exercises the actual I2C source against
a recording HAL boundary for Apollo3-style and newer instruction layouts, with
validation enabled and disabled. It checks combined transaction descriptors,
input limits, native failures and legacy helper behavior. These host checks do
not emulate I2C waveforms or qualify vendor HAL recovery.

`test_tempco_lifecycle.py` runs the actual AP4 TempCo and timer adapters against
recording HAL calls. It sweeps initialization failures, exercises cleanup
failures and retries, rejects timer ownership conflicts, and checks callback
suppression before setup completes and during release. ADC deinitialization
failure deliberately requires reset because the vendor HAL may discard its
handle on error. The `tempco-smoke` fixture provides a bounded AP4 KXR
setup/release/reallocation check through the shared hardware build/session API;
it does not validate temperature samples or applied trims.
