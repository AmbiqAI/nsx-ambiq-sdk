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
`apollo4b_evb`, `apollo330mP_evb`, `apollo510_evb`, and `apollo510b_evb`. These are
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

The worker flashes an ELF using SEGGER, resets and halts, clears/verifies the
result buffer, poisons and reads back all three startup probe variables, and
resumes normal reset entry. It never sets the PC to `main`
or `Reset_Handler`. Stale SRAM is not accepted as a fresh result.

Firmware asserts initialized data, zeroed BSS, and a C++ constructor's effect.
Their starting values are deliberately wrong, so retained correct RAM cannot
satisfy these assertions. Sentinel addresses come from the built ELF and are
recorded in its manifest; `seeded.json` records verified pre-execution values.
Unity output and counts are stored in a bounded SRAM record. The host requires
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
