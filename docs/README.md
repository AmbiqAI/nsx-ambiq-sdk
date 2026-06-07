# Maintainer Docs

These docs are for contributors maintaining the unified NSX AmbiqSuite SDK repo. User-facing usage should stay in the root `README.md` and module-level READMEs.

## Current State

- `platform-coverage.md` lists staged, descriptor-only, and planned SoC/board coverage.
- `r5-sdk-intake-report.md` records the current AmbiqSuite R5.2.0 intake and artifact build status.
- `split-repo-exit-checklist.md` tracks what must be true before the individual R3/R4/R5 SDK repos can be retired.
- `toolchain-smoke.md` describes opt-in local/CI link smokes for staged toolchains.

## Intake And Scope

- `sdk-drop-workflow.md` describes how to ingest a new AmbiqSuite drop and promote approved artifacts.
- `sdk-drop-manifest.example.yaml` is the manifest shape for future SDK-drop audits.
- `core-sdk-scope.md` defines what belongs in this core SDK bundle.

## Internal Reviews

- Use tests and module READMEs for current module-boundary rules.
- Put new design notes here only when they describe an active maintainer contract.

Keep these files short and factual. If a note no longer guides a maintainer action or contract, remove it.
