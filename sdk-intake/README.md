# SDK Intake Helpers

This folder is for maintainers updating the curated AmbiqSuite provider payloads.
It is not part of the normal end-user workflow.

The helper in this directory rebuilds and promotes approved AmbiqSuite HAL/BSP
artifacts into the source-controlled provider module:

```text
artifacts/ambiqsuite/<train>/<version>/...
modules/nsx-ambiqsuite/sdk/
```

Local raw inputs stay under the ignored intake workspace:

```text
sdk-intake/local/drops/
sdk-intake/local/work/
```

## What The Helper Does

- materializes an AmbiqSuite source tree from one of:
  - `--source-root`
  - `--zip`
  - `--ambiqsuite-repo` plus `--source-ref` (default ref: `stable`)
- builds HAL/BSP archives for every released part/board and the selected toolchain(s)
- writes an artifact manifest under `artifacts/ambiqsuite/<train>/<version>/manifest.yaml`
- promotes approved headers, system sources, utility sources, and prebuilt HAL/BSP archives into `modules/nsx-ambiqsuite/sdk/`

It does not bundle examples, FreeRTOS, TinyUSB, Cordio, CMSIS-NN, or CMSIS-DSP.

## Strict Inputs

The helper intentionally does **not** assume machine-specific toolchain or SDK
paths.

- For the git-ref workflow, pass `--ambiqsuite-repo /path/to/ambiqSuite` or set `AMBIQSUITE_REPO`.
- For `--toolchain atfe`, pass `--atfe-root /path/to/ATfE` or set `ATFE_ROOT`.
- For `--toolchain acfe`, pass `--acfe-root /path/to/ACfE` or set `ACFE_ROOT`.

If an input is missing, the helper exits with an explicit error instead of
guessing a local filesystem path.

## Unified Provider Flow

The intake path rebuilds the single `nsx-ambiqsuite` provider from one rolling
AmbiqSuite ref, typically `stable`, mirroring upstream. One build covers every
released Apollo-class part and board:

- Apollo2 (toolchains `gcc`, `atfe`)
- Apollo3 / Apollo3P (toolchains `gcc`, `atfe`, `acfe`)
- Apollo4L / Apollo4P (toolchains `gcc`, `atfe`, `acfe`)
- Apollo330P / Apollo510 / Apollo510L (toolchains `gcc`, `atfe`, `acfe`)

Apollo2 predates ACfE, so it is built for `gcc` and `atfe` only; the build skips
the unsupported `(apollo2, acfe)` pair automatically. The provider train id is
`stable`.

Snapshot versions are derived automatically from the source ref and commit date,
for example `stable-2026.06.17`.

## Simple Examples

Build one toolchain from a local AmbiqSuite git checkout:

```bash
python sdk-intake/build_ambiqsuite.py \
  --toolchain gcc \
  --ambiqsuite-repo /path/to/ambiqSuite \
  --source-ref stable
```

Build ATfE:

```bash
python sdk-intake/build_ambiqsuite.py \
  --toolchain atfe \
  --ambiqsuite-repo /path/to/ambiqSuite \
  --source-ref stable \
  --atfe-root /path/to/ATfE
```

Build all supported toolchains:

```bash
python sdk-intake/build_ambiqsuite.py \
  --toolchain all \
  --ambiqsuite-repo /path/to/ambiqSuite \
  --source-ref stable \
  --atfe-root /path/to/ATfE \
  --acfe-root /path/to/ACfE
```

Restrict a build to specific part(s) with `--only-part` (repeatable); promotion
still requires the full payload to exist on disk for the version.

Promote an already-built payload without rebuilding:

```bash
python sdk-intake/build_ambiqsuite.py \
  --promote-only \
  --ambiqsuite-repo /path/to/ambiqSuite \
  --source-ref stable
```

## Update Steps Before A PR

1. Rebuild or re-promote the provider payload.
1. Confirm provider payload diffs are intentional under `modules/nsx-ambiqsuite/sdk/`.
1. Refresh provider metadata if the source snapshot changed.
1. Run the contract tests:

```bash
python -m pytest tests/test_cmake_contract.py -q
```

1. Review any policy changes, such as supported toolchains or board exposure.

## Notes

- Artifacts are release-style by default; the helper appends `-g0` after the
  native AmbiqSuite makefile flags. Use `--debug-symbols` only for local
  diagnostic builds.
- Single-toolchain reruns rebuild only that toolchain's native output and update
  only that toolchain's staged artifacts.
- Apollo2 intentionally does not support `acfe` / `armclang` because AmbiqSuite
  never shipped armclang startup/linker support for it; the build skips that
  part/toolchain pair automatically.
