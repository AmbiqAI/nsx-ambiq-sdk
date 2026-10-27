# Local SDK hardware tests (initial PoC)

The SDK owns functional test firmware and acceptance criteria. pytest runs the
host orchestration; pinned Unity 2.6.1 runs assertions on the target. No GitHub
runner, Helia installation, microphone, or external wiring is needed for the
startup case. Helia code and infrastructure are unchanged.

This first phase tests the SDK's shipped CMake board/startup/HAL/BSP integration.
It does **not** yet test neuralspotx's public install/registry/vendoring workflow.
That requires a separate generated-app acceptance test. No implicit sibling-repo
lookup is used. The tested SDK is this checkout, including any local edits.

## Environment

Initial hardware host: Linux, SEGGER J-Link software installed with USB access,
Arm GNU toolchain, CMake, Ninja, Python >=3.10, and uv. Install SEGGER according to
its license; it is not bundled. `pylink-square` loads its shared library.

```sh
uv sync --locked --group ci --group hardware
uv run --group ci pytest -q tests/test_hardware_runner.py
```

Unity's three source files and MIT license are vendored under
`tests/hardware/vendor/unity`, pinned to a full upstream commit with SHA-256
checks. Builds never download dependencies. Python versions are resolved by
`uv.lock`; record the compiler version separately. Nix is not required. A future
optional Nix shell must preserve these same commands and account for the
separately installed SEGGER/ATfE tools.

### Apollo330 Plus / Apollo510 Lite device pack

These devices require Ambiq's interim `JLink_Apollo330P_510L_20251015.zip`
and J-Link 8.40 or later. Obtain the archive from the SharePoint tools folder
linked by [Ambiq's installation guide](https://ambiqmicro.atlassian.net/wiki/spaces/C/pages/3286630710),
then provision it once for the user running the tests:

```sh
uv run --group hardware python -m tools.hardware.device_pack \
  --archive /path/to/JLink_Apollo330P_510L_20251015.zip
```

The public repository contains the source reference, supported device names,
and archive/file SHA-256 pins in `tools/hardware/device-pack.json`. The supplied
archive contains no redistribution license, so its vendor binaries are not
committed or relicensed here. Provisioning extracts only pinned members into
`${XDG_CACHE_HOME:-~/.cache}/nsx-sdk/device-packs/<archive-sha256>`.
An existing cache entry must verify; remove a damaged entry before reprovisioning.

Before opening a probe for an affected device, the worker verifies every pack
file and selects the directory using SEGGER's `JLinkDevicesXMLPath` command.
It does not modify the SEGGER installation or user-wide device database, change
device names, or download anything during a test. Missing packs fail with the
provisioning command. Other devices require no pack. Each hardware run writes
`jlink.json` with the library version and selected pack path/checksums.

Local hosts and future board-farm runners use the same provisioning command;
runners can obtain the pinned ZIP from an internal artifact store. Changing a
pack is an explicit manifest update followed by hardware qualification.

## Build without hardware

```sh
uv run --group hardware --group ci pytest -q -rs tests/hardware \
  --build-only --board apollo3p_evb \
  --gcc-root /usr/local/arm-gnu-toolchain
```

`--gcc-root` can be omitted if `arm-none-eabi-gcc` is on PATH. Build success is
reported as a **skipped execution**, with an evidence path, never a hardware
pass. Compile errors fail the run. Missing requested tools are errors, not skips.

The initial selectable boards are `apollo3_evb`, `apollo3p_evb`, `apollo4p_evb`,
`apollo4p_blue_kxr_evb`, `apollo4p_blue_kbr_evb`, `apollo330mP_evb`, `apollo510_evb`, and `apollo510b_evb`. These are
exact SDK descriptor names, not a claim that all have been physically qualified.
CMake consumes the existing board/SoC definitions; the test does not duplicate
pin, clock, memory or compiler-flag tables. Only GCC is implemented initially.

## Run locally

Reserve the board through your lab's existing allocation mechanism first. In
Ambiq's shared lab, use the existing hardware claim service and release the
claim afterward. The SDK's per-user Linux advisory lock only prevents concurrent
**SDK** sessions on the same probe; it does not coordinate Helia, other users,
other hosts, or arbitrary debugger programs. SEGGER also owns the probe while
connected. Do not use this PoC on a board assigned to another job.

Use the exact probe serial from your physical board inventory. SEGGER device and
SWD speed come from the SDK board/SoC descriptors; optional `--device` checks
that your inventory agrees and refuses mismatches before touching hardware:

```sh
uv run --group hardware --group ci pytest -q tests/hardware \
  --hardware --board apollo3p_evb --probe YOUR_SERIAL --device AMA3B2KK-KBR \
  --gcc-root /usr/local/arm-gnu-toolchain \
  --junitxml=artifacts/hardware/junit.xml
```

This **replaces the application firmware** on the selected board. No auto-probe
selection or fallback occurs. The caller must ensure the physical board matches
the requested descriptor and device; chip-part auto-verification is not yet
implemented. The runner does not modify power jumpers or control lab power.

Normal `pytest` skips the opt-in hardware test without building or opening a
probe. Explicit `--hardware` requires a board and serial. Each run gets
its own artifact directory; the result includes a new build identity.

## Execution and evidence

The worker resets and halts before programming an ELF using SEGGER, resets and
halts again, clears/verifies the
result buffer, poisons and reads back all three startup probe variables, and
resumes normal reset entry. It never sets the PC to `main`
or `Reset_Handler`. Stale SRAM is not accepted as a fresh result.

Firmware asserts initialized data, zeroed BSS, and a C++ constructor's effect.
Their starting values are deliberately wrong, so retained correct RAM cannot
satisfy these assertions. Sentinel addresses come from the built ELF and are
recorded in its manifest; `seeded.json` records verified pre-execution values.
Unity output and counts are stored in a bounded SRAM record. The host first
reads the aligned completion word, then reads the payload only after completion,
so bytewise reads cannot race the producer while it publishes the header. The host requires
the expected protocol, matching build identity, all three named cases exactly
once, zero failures/ignores, a Unity summary, and final completion. Output
overflow, missing completion, or a transport disconnect cannot pass.

The J-Link worker runs in a subprocess with a wall-clock limit (capture timeout
plus 60 seconds for connect/flash/cleanup). The capture timeout defaults to 15
seconds and is adjustable with `--hardware-timeout`. SIGTERM, SIGINT (Ctrl-C), or timeout stops the worker process group, waits up to
three seconds, then kills/reaps it if necessary. The worker inherits the probe
lock descriptor. Closing the parent's descriptor cannot release the lock while
the worker is still alive. SIGKILL cannot be handled: if the parent is killed
abruptly, the worker may continue, but retains the lock until it exits. In that
case no final execution report is guaranteed; an operator may need to stop the
orphaned worker before using the probe. Firmware may remain running
on the board; the runner does not restore the previous application.

This is a **debugger-connected reset test**, not cold boot or sleep qualification.
Reading SRAM can affect execution. RTT/UART, standalone power-cycle testing,
cache-aware reporting, and tests that must run without an attached debugger
need separate transport/reset policies before those claims are made.

Each run saves the build log, manifest (SDK revision/dirty state, compiler,
firmware hash, source/library hashes), execution metadata, last raw result,
Unity output and parsed result on success, and failure details on error. JUnit
XML is produced by pytest. Build errors retain their log. Build identity is a
fresh-run identifier; ELF SHA-256 records the actual binary.

## Test the tester

Hardware-free parser tests cover missing/duplicate cases, assertions, ignored
tests, wrong identity, incomplete/unknown protocol, output overflow, timeout,
disconnect and probe-lock cleanup. Real subprocess tests exercise SIGTERM,
SIGINT, timeout escalation, reaping, and retained locking after parent SIGKILL. Unity vendoring is integrity-checked.

To demonstrate actual failures on a reserved board, add either:

```text
--fault-injection assertion
--fault-injection timeout --hardware-timeout 2
```

Both commands must fail pytest, never produce a pass. The manifest records the
injection mode. Finish with a normal run so the board contains passing firmware.
Additional AP3P-only controls remove actual operations from an artifact copy
of the shipped startup source (the SDK source is never edited):

```text
--fault-injection omit-data
--fault-injection omit-bss
--fault-injection omit-constructors
```

Each must fail its corresponding Unity assertion after verified sentinel
poisoning. Mutations require exactly one known operation; changed source
structure is an error rather than an assumed successful mutation. These modes
are restricted to `apollo3p_evb` until other startup implementations have audited
mutation points. The modified source is included in the evidence hashes.

These controls exist solely to verify the runner, not to qualify SDK behavior.

## Subsequent phases

1. Verified physical board inventory and qualification evidence across the exact
   AP3/AP4/AP330/AP510 variants available in the lab.
2. Integration with the shared board allocator, and stable neuralspotx API-based
   generated-app acceptance. Avoid importing Helia private APIs or copying its
   kernel-testing pipeline.
3. Clock/timebase and sleep/reset cases with independent reference measurements
   and supported per-part operating modes; DMA/cache and audio tests afterward.
4. A manual GitHub workflow invoking the same local commands, with required-board
   coverage enforced. A skipped required test must make qualification incomplete.

Do not interpret the initial startup result as qualification of every linked
HAL/BSP driver, all runtime memory regions, or peripheral behavior.

## Required qualification profiles

For an update acceptance run, select an exact board/GCC startup profile:

```sh
uv run --group hardware --group ci pytest -q tests/hardware \
  --qualification-profile apollo3p_evb-gcc-startup \
  --qualification-report artifacts/hardware/ap3p-qualification.json \
  --hardware --probe YOUR_SERIAL --gcc-root /usr/local/arm-gnu-toolchain
```

The profile supplies the board; an explicit conflicting `--board` is rejected.
Profiles exist for each selectable descriptor listed above, named
`<descriptor>-gcc-startup`. Their existence does not mean a board has passed.
This first profile requires only the startup suite, not full SDK qualification.
The typed suite registry records expected firmware cases; future suites supply
a suite specification and firmware sources using the shared builder/reporter,
then add an explicit profile entry. New transport requirements need separate work.

A fresh JSON report lists required suites, pytest setup/call/teardown outcomes,
and evidence directories. Skipped, deselected, uncollected or interrupted cases
are incomplete. Failed assertions or fixture teardown fail qualification.
Build-only runs remain useful, but cannot satisfy a hardware profile. Missing
coverage returns nonzero even when ordinary pytest would accept skips. Fault
injection is deliberately refused in qualification mode. Each invocation needs
its own report path when running concurrently. Automation must require a zero
process exit status and a passing report from that invocation; a report file
alone is not acceptance evidence. Argument parsing can fail before report
initialization. An uncatchable process kill may
leave the initial incomplete report; it cannot establish qualification.

Firmware manifests also hash the packaged SDK provider metadata, artifact
manifest and SDK version header. Together with linked-input hashes, these tie
results to the candidate SDK instead of just a release label.

These are ordinary local pytest commands. A future board-farm workflow should
reserve hardware, invoke the same commands and upload evidence; it should not
implement separate acceptance logic. The existing manual lab reservation
requirement still applies.

### SDK update coverage inventory

Extend these existing checks before introducing overlapping test suites:

| Boundary | Existing coverage | Remaining physical/update evidence |
| --- | --- | --- |
| Vendor ABI and artifact selection | `test_vendor_abi.py`, `test_artifact_baseline.py` | Candidate toolchain/board matrix with required-tool enforcement |
| Intake and maintained patches | `test_sdk_intake.py`, `test_intake_workflow.py`, `test_committed_intake_patches.py`, `test_intake_patches_byte_for_byte.py` | Current/candidate comparison and reviewed patch disposition |
| Startup/linker integration | `test_cpp_startup.py`, `test_shared_startup.py`, `test_scatter_load_bounds.py`, `test_link_smoke.py` | Startup profile execution on each exact required EVB |
| Clock/performance adapter | `test_power_core_clock.py` | Supported mode transitions on silicon; independent frequency evidence separately |
| Audio adapter | `test_audio_buffers.py`, `test_audio_lifecycle.py` | Target integration where needed; microphone checks remain optional |
| Memory configuration | `test_small_m55_memory.py`, `test_psram_api.py` | Relevant target placement/runtime checks; no blanket PSRAM qualification |

This inventory identifies existing tests, not a claim that they exhaustively
cover each boundary. The next PR should audit their assertions against prior
fixes and candidate SDK changes, and specify required versus optional tools.

## Hardware-free SDK compatibility gate

Before scheduling board tests, run from the candidate SDK checkout root:

```sh
uv run --group ci python -m tools.sdk_compatibility \
  --profile gcc --output artifacts/compatibility/candidate-gcc
```

The output directory must be new. Preserve it as evidence; choose a different
name for each attempt. Install Arm GCC using the same `GCC_ROOT`/PATH discovery
as the existing SDK tests. Git, CMake, Ninja and the usual host-test dependencies
must be available. Unset `PYTEST_ADDOPTS`; acceptance must not inherit test
selection or execution options from a developer shell.

The `payload` profile checks the artifact hash/inventory baseline, committed
patches and release-baseline reconstruction, intake behavior, CMake contracts
and module manifests. It requires full release history/tags for reconstruction.
The `gcc` profile includes those checks and additionally requires:

| Real compiler check | Required minimum cases |
| --- | ---: |
| Vendor type ABI: C/C++, default/ATfE artifact selection/conflicting flags | 6 |
| Complete linked firmware: AP330MP, AP510, AP510B, AP510DL | 4 |
| Audio adapter against real headers: AP3, AP3P and four M55 variants | 6 |
| Performance adapter against real HAL headers: four M55 variants | 4 |

The GCC profile also requires the following existing integration regressions:

| Integration boundary | Required minimum cases |
| --- | ---: |
| AP3P/AP4 KBR/AP4 KXR startup plus retained real HAL/BSP implementations | 3 |
| Runtime/heap/SVC firmware: AP3P, AP4 KBR/KXR, AP330P, AP510, AP510B | 6 |
| Apollo2/3/3P constructors, default and application CRT | 6 |
| Audio buffer/packing models, Apollo3 and Apollo5 | 48 |
| Audio cache-view model | 2 |
| Applicable Apollo3/Apollo5 lifecycle fault scenarios | 66 |
| Clock/performance model: six targets, sixteen scenarios each | 96 |

The AP3P/AP4 link fixture reuses the hardware startup source and shared builder.
It retains a link-only probe that pulls actual PDM HAL and low-power BSP
implementations into the ELF; merely naming libraries in CMake is insufficient.
The probe is never called. No microphone, probe, external wiring, or device pack
is required. These builds do not qualify physical PDM or BSP operation.

Audio lifecycle cases are collected only for applicable backend operations;
there is no acceptance exception for skipped tests. Their module-scoped
parametrization builds one sanitizer executable per backend. Audio backend/scenario
and backend/phase/fault combinations, clock target/scenario combinations, and
constructor/CRT combinations are required explicitly, in addition to minimum
counts and required boards. Acceptance matrices are kept independent of test
parametrization so replacing a test with a duplicate cannot hide lost coverage. The payload-only profile remains unchanged.

ATfE/ArmClang execution is outside this GCC profile and is
listed explicitly as excluded, not reported as passed. Selecting an ATfE
*archive* in a GCC ABI check does not mean the ATfE compiler was exercised.

Every selected case must pass setup, execution and teardown. Skips (including
missing compilers/history), collection errors, empty coverage and reduced
required matrix counts prevent qualification. Existing normal pytest behavior
is unchanged. Report consumers must require both process exit zero and
`status: passed` in this invocation's `compatibility.json`.

Evidence includes the candidate revision/dirty state, SDK identity hashes,
compiler identity for GCC, exact cases, exclusions, requirements, JUnit XML,
and retained pytest build directories. This is a compatibility check of the
checkout, not physical board qualification or approval to promote an SDK.

For the next intake, stage the candidate in an isolated checkout, review any
patches that upstream has absorbed, and deliberately update release-baseline
expectations where appropriate. Do not delete a failing patch check simply to
make an update pass. Run current and candidate with the same test definitions
and toolchain; automated comparison and board aggregation remain later work.

## Test infrastructure boundaries

`tools/hardware/api.py` is the local Python entry point. `BuildRequest` describes
one explicit suite/board/toolchain build and returns a typed `Firmware` artifact.
Build output must be new or empty, so prior CMake options cannot leak into a run.
`open_board(BoardRequest(...))` owns a local probe lease for a scoped session;
`session.run(firmware, output)` returns a decoded `Result`. Closing the session
prevents subsequent use. Worker cancellation/reaping remains inside the runner,
and the child inherits the lease until it exits. Shared-lab reservation is still
the caller's responsibility.

The pytest adapter separates responsibilities into function-scoped fixtures:

- `run_config`: validate invocation and resolve the toolchain.
- `firmware`: build the selected `suite_spec` and register its evidence path.
- `board_session`: acquire/release access after building; skip before acquiring
  anything in build-only mode.
- `suite_result`: run once and return validated target evidence.

The startup test supplies its suite definition from the hardware tests' conftest.
The shared builder/transport does not choose a startup suite by default. A suite
specifies its source directory, ELF name, expected Unity cases, optional memory
seeds and narrowly scoped build preparation. Addresses are resolved from ELF
symbols. The startup suite alone defines its three poison values and AP3P-only
omission controls. Suite case names must be nonempty, unique C identifiers.

`tests/hardware/common/firmware.cmake` provides the shared SDK/CMake target setup.
`result_reporter.cpp` owns the bounded result buffer, Unity output and completion
publication. Each suite owns assertions and Unity setup/teardown. The SRAM wire
protocol remains version 1; build manifests are version 2 and record generic
`seeds` and `expected_cases`, replacing the startup-specific sentinel map.

Hardware qualification and the host compatibility gate share the pure completion
rules in `tools/test_evidence.py`. Their evidence formats and orchestration remain
separate. Compatibility requirements now include exact ABI parameter combinations,
not just a minimum count; adaptation of the existing firmware fixture tuple to
named dimensions is isolated in `case_parameters`.

`tests/fixtures/hardware_second` is a synthetic infrastructure check, not another
board qualification suite. It builds C firmware with two cases, one seed and a different ELF
name, alongside the C++ startup suite. Host tests exercise the worker CLI, simulated probe, parser, parent validation
and session cleanup with this contract, including execution with zero seeds.
They establish reusable plumbing, not physical behavior of the second suite.

Board/silicon facts continue to come from SDK descriptors. The explicit board
allowlist and required matrices define reviewed coverage rather than discovering
whatever happens to be available. Additional families, compiler support, transports
and lab allocation integration require explicit implementation and validation.

Qualification reports use schema version 2. A required test's suite specification
must exactly match its profile before building. Success also requires a bound
build and matching successful execution metadata plus a complete raw result for
that build identity and case set. Passing pytest phases alone are incomplete.
Reports include the bound build ID alongside the evidence directory.

The local session API resolves execution output paths against the caller's working
directory before starting the worker. Unity initialization receives the suite's
source filename explicitly, so failures point to the assertion's source file
rather than the shared reporter.

Apollo510B selects the SEGGER `AP510BFA-CBR` device profile; Apollo510 continues
to select `AP510NFA-CBR`. Their programming profiles are not interchangeable.
The explicit reset before programming establishes a known state for the flash
loader even when an earlier application or failed programming attempt left the
core in a different state.

## Core runtime qualification

Use `<descriptor>-gcc-core` to require both startup and runtime suites (seven
Unity cases total). Existing `-gcc-startup` profiles still require only startup.

```sh
uv run --group hardware --group ci pytest -q tests/hardware \
  --hardware --qualification-profile apollo510b_evb-gcc-core \
  --probe YOUR_SERIAL --gcc-root /usr/local/arm-gnu-toolchain \
  --qualification-report artifacts/hardware/core-report.json
```

The runtime suite checks:

- MSP alignment at an ABI call boundary and a local variable within the linked
  `.stack` section. Stack bounds come from linker expressions, not copied maps.
- Two small malloc allocations: bounds within `__HeapBase`/`__HeapLimit`, required
  alignment, writable independent contents, and separation from the stack.
- The NSX `_sbrk` boundary: an over-limit request fails with `ENOMEM` without
  advancing the break. This is not a heap-exhaustion stress test.
- The SVC vector points to the test handler, the real `svc` instruction dispatches
  through that vector, and the handler observes the expected exception number.
  Interrupt masks are restored before assertions. No peripheral wiring is needed.

`tests/hardware/runtime` owns these cases. Common build/report/transport code is
unchanged by the suite definition. Build evidence also hashes the source files
listed by CMake's compile database, including the NSX heap implementation.

For a negative control on a reserved board:

```sh
uv run --group hardware --group ci pytest -q tests/hardware/test_runtime.py \
  --hardware --board apollo3p_evb --probe YOUR_SERIAL \
  --gcc-root /usr/local/arm-gnu-toolchain --fault-injection runtime-no-svc
```

This suppresses the handler's observation while preserving actual dispatch;
the SVC assertion must fail. Do not supply a qualification profile for deliberate
fault injection. Finish with a normal run. Fault names are validated by the
selected suite before building rather than maintained in a shared CLI list.

### Physical evidence from 2026-10-23

| Probe | Configuration | Result |
| --- | --- | --- |
| 1160000172 | Apollo3 Blue Plus / `apollo3p_evb` | Startup and runtime passed; SVC negative control failed the expected assertion |
| 1160001491 | Apollo4 Blue Plus / KBR and KXR descriptors | Both configurations passed core tests; repeated NSX UART hello identifies KXR wiring (GPIO12 TX), with no UART output under KBR (GPIO60 TX) |
| 1160002255 | Apollo510 / `apollo510_evb` | Startup and runtime passed after the pre-flash reset fix |
| 1160002954 | Apollo510B / `apollo510b_evb` | Startup and runtime passed with corrected probe profile/reset/completion read; SVC negative control failed as expected |
| 1160003058 | Apollo330 Plus / `apollo330mP_evb` | Startup and runtime passed using J-Link 9.88 with Ambiq overlay `JLink_Apollo330P_510L_20251015.zip` and descriptor device `AP330MPA-CBR` |

These are debugger-connected GCC runs. Core tests alone do not distinguish AP4
variants. A separate NSX UART hello diagnostic on probe 1160001491 produced five
messages with KXR (GPIO12 TX) and none with KBR (GPIO60 TX), repeated twice.
Both variants returned successful initialization status. Use `apollo4p_blue_kxr_evb`
for this board; this verifies the console routing, not every peripheral mapping.
The initial AP330 attempt stopped before programming because its device pack was
missing. Installing Ambiq's `JLink_Apollo330P_510L_20251015.zip` into a separate
J-Link 9.88 installation resolved device recognition without changing the SDK
descriptor or substituting a device profile. All seven cases then passed. The
run selected that installation with `LD_LIBRARY_PATH`; the system J-Link 9.50
installation remained unchanged. No cold-boot, timing accuracy,
DMA/cache, sleep or peripheral qualification is claimed. Successful final runs
leave test firmware on the tested boards; claims are released afterward.

A follow-up AP330 run passed all seven cases with the unchanged system J-Link
9.50 library and the checksum-pinned cache pack selected by the worker. Its
`jlink.json` records version 9.50 and the archive/file hashes; no installed
AP330 part-name definition or `LD_LIBRARY_PATH` override was required.

### Fresh-checkout baseline

A separate clean checkout at `3a466ade04eb526dc7e2c78e3e90c7d679df4dcf` with a new
virtual environment and empty device-pack cache passed locked dependency setup,
AP330 build-only checks, the missing-pack diagnostic, archive provisioning, and
all seven AP330 hardware cases. The GCC compatibility gate passed 288 checks
with 40 other-compiler cases deselected. Reports, binaries, UART diagnostic
sources/captures and `board-baseline.json` are retained in the lab evidence
directory `artifacts/nsx/sdk-core-qualification-2026-10-23` outside this repo.

## Clock/timebase next increment

The [two-board clock/timebase contract](clock-timebase-qualification.md) records
the first milestone, source evidence, transition matrix and unresolved reference
prerequisites. Its host-tested counter arithmetic does not constitute a hardware
qualification profile or new physical evidence.

Experimental AP510B/AP330 clock and SysTick work is documented in
[Clock lab tests](clock-lab-tests.md); it is not part of the core qualification profiles.

Use [fixed-harness comparison](sdk-harness-comparison.md) to build and run the same tests against two explicit SDK checkouts.

### Timer-period smoke

`tests/hardware/test_timer.py` uses the `timer-period` suite on AP4 Blue Plus
KXR, AP330 Plus, AP510 and AP510B. Use the same local `--hardware`, `--board`,
`--probe`, `--gcc-root` and artifact options as the other suites. It checks
1/10/50 ms callback intervals against DWT CPU cycles after LOW-mode startup,
records seven intervals per period, and stops/unregisters the timer afterward.
A 20% window catches conversion errors without claiming calibrated HFRC
accuracy. This does not test sleep, cold boot, long callbacks or IRQ load.
AP4L/AP510L have host-side cross-compile coverage only.
