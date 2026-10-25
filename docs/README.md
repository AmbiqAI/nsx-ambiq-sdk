# Maintainer Docs

These docs are for contributors maintaining the unified NSX AmbiqSuite SDK repo. User-facing usage should stay in the root `README.md` and module-level READMEs.

## Current State

- This repo carries a single SDK provider (`nsx-ambiqsuite`) spanning the Apollo2, Apollo3, Apollo4, and Apollo5 families, with HAL/BSP shipped as prebuilt static libraries under `modules/nsx-ambiqsuite/sdk/lib/<toolchain>/`.
- Per-SoC facts live in `cmake/socs/facts/<skew>.cmake` (the single source of truth, including SEGGER/J-Link defaults) and are loaded by `nsx_load_soc_facts()`; see [../cmake/README.md](../cmake/README.md).
- `platform-coverage.md` lists staged, descriptor-only, and planned SoC/board coverage.
- `toolchain-smoke.md` describes opt-in local/CI link smokes for staged toolchains.
- `small-m55-memory.md` records the Apollo330 Plus/Apollo510 Lite physical and
  application memory limits, source conflicts, and supported linker profiles.
- `core-clock.md` records the part-specific performance clock sources and the
  boundary between CMSIS clock synchronization and application-owned timing.

- `clock-timebase-qualification.md` specifies the first two-board clock/timebase
  milestone, reference prerequisites and target counter arithmetic.
- `local-hardware-tests.md` describes the opt-in local pytest/Unity startup harness.

## Hardware testing

- [Experimental clock/SysTick lab checks](clock-lab-tests.md)
- [Fixed-harness SDK comparison](sdk-harness-comparison.md)

## Intake And Scope

- `sdk-drop-workflow.md` describes how to ingest a new AmbiqSuite drop and promote approved artifacts.
- `intake-hardening.md` describes the optional staged intake workflow (`sdk-intake/intake_workflow.py`): staging, reviewable diffs, ownership/hash verification, the patch hook, and the golden-baseline verification path.
- `sdk-drop-manifest.example.yaml` is the manifest shape for future SDK-drop audits.
- `core-sdk-scope.md` defines what belongs in this core SDK bundle.
- `versioning.md` defines distribution, API-version, and immutable-tag policy.
- `source-ownership.md` defines generated, upstream-derived, and NSX-owned paths.
- `release-process.md` is the concise manual release checklist.
- `acfe-artifact-manifest-forensics.md` records the `v5.2.23` ACfE archive /
  artifact-manifest mismatch, its root cause, and the `5.2.24` correction.

## Internal Reviews

- [AP3/AP4 startup policy proposal](power-startup-policy.md) defines the proposed
  startup ownership and power-field contract for #137; it is not yet enforced.

- Use tests and module READMEs for current module-boundary rules.
- Put new design notes here only when they describe an active maintainer contract.

Keep these files short and factual. If a note no longer guides a maintainer action or contract, remove it.

- [vendor-abi.md](vendor-abi.md) — selected archive ABI and standalone consumer checks.


- [scatter-load-bounds.md](scatter-load-bounds.md) — aggregate NVM scatter load bounds and real-linker checks.

- [shared-startup.md](shared-startup.md) — shared/resource startup copy windows and validation.

- [i2c-combined-transfers.md](i2c-combined-transfers.md) — repeated-START wrapper mapping, limits, failure handling and validation.
