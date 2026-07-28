# Maintainer Docs

These docs are for contributors maintaining the unified NSX AmbiqSuite SDK repo. User-facing usage should stay in the root `README.md` and module-level READMEs.

## Current State

- This repo carries a single SDK provider (`nsx-ambiqsuite`) spanning the Apollo2, Apollo3, Apollo4, and Apollo5 families, with HAL/BSP shipped as prebuilt static libraries under `modules/nsx-ambiqsuite/sdk/lib/<toolchain>/`.
- Per-SoC facts live in `cmake/socs/facts/<skew>.cmake` (the single source of truth, including SEGGER/J-Link defaults) and are loaded by `nsx_load_soc_facts()`; see [../cmake/README.md](../cmake/README.md).
- `platform-coverage.md` lists staged, descriptor-only, and planned SoC/board coverage.
- `toolchain-smoke.md` describes opt-in local/CI link smokes for staged toolchains.

## Intake And Scope

- `sdk-drop-workflow.md` describes how to ingest a new AmbiqSuite drop and promote approved artifacts.
- `sdk-drop-manifest.example.yaml` is the manifest shape for future SDK-drop audits.
- `core-sdk-scope.md` defines what belongs in this core SDK bundle.
- `versioning.md` defines distribution, API-version, and immutable-tag policy.
- `source-ownership.md` defines generated, upstream-derived, and NSX-owned paths.
- `release-process.md` is the concise manual release checklist.

## Internal Reviews

- Use tests and module READMEs for current module-boundary rules.
- Put new design notes here only when they describe an active maintainer contract.

Keep these files short and factual. If a note no longer guides a maintainer action or contract, remove it.
