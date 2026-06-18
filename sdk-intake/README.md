# SDK Intake Helpers

This folder is for maintainers updating the curated AmbiqSuite provider payloads.
It is not part of the normal end-user workflow.

The helper in this directory rebuilds and promotes approved AmbiqSuite HAL/BSP
artifacts into the source-controlled provider modules:

```text
artifacts/ambiqsuite/<train>/<version>/...
modules/nsx-ambiqsuite-r2/sdk/
modules/nsx-ambiqsuite-r3/sdk/
modules/nsx-ambiqsuite-r4/sdk/
modules/nsx-ambiqsuite-r5/sdk/
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
- builds HAL/BSP archives for the selected train and toolchain(s)
- writes a per-train artifact manifest under `artifacts/ambiqsuite/<train>/<version>/manifest.yaml`
- promotes approved headers, system sources, utility sources, and prebuilt HAL/BSP archives into `modules/nsx-ambiqsuite-r*/sdk/`

It does not bundle examples, FreeRTOS, TinyUSB, Cordio, CMSIS-NN, or CMSIS-DSP.

## Strict Inputs

The helper intentionally does **not** assume machine-specific toolchain or SDK
paths.

- For the git-ref workflow, pass `--ambiqsuite-repo /path/to/ambiqSuite` or set `AMBIQSUITE_REPO`.
- For `--toolchain atfe`, pass `--atfe-root /path/to/ATfE` or set `ATFE_ROOT`.
- For `--toolchain acfe`, pass `--acfe-root /path/to/ACfE` or set `ACFE_ROOT`.

If an input is missing, the helper exits with an explicit error instead of
guessing a local filesystem path.

## Current Multi-Train Flow

The current intake path rebuilds provider trains from a single rolling
AmbiqSuite ref, typically `stable`.

- `r2`: Apollo2, toolchains `gcc` and `atfe`
- `r3`: Apollo3/Apollo3P, toolchains `gcc`, `atfe`, `acfe`
- `r4`: Apollo4L/Apollo4P, toolchains `gcc`, `atfe`, `acfe`
- `r5`: Apollo330P/Apollo510/Apollo510L, toolchains `gcc`, `atfe`, `acfe`

Snapshot versions are derived automatically from the source ref and commit date,
for example `stable-2026.06.17`.

## Simple Examples

Build one toolchain for one train from a local AmbiqSuite git checkout:

```bash
python sdk-intake/build_ambiqsuite.py \
  --train r4 \
  --toolchain gcc \
  --ambiqsuite-repo /path/to/ambiqSuite \
  --source-ref stable
```

Build ATfE for a legacy train:

```bash
python sdk-intake/build_ambiqsuite.py \
  --train r2 \
  --toolchain atfe \
  --ambiqsuite-repo /path/to/ambiqSuite \
  --source-ref stable \
  --atfe-root /path/to/ATfE
```

Build all supported toolchains for a train:

```bash
python sdk-intake/build_ambiqsuite.py \
  --train r5 \
  --toolchain all \
  --ambiqsuite-repo /path/to/ambiqSuite \
  --source-ref stable \
  --atfe-root /path/to/ATfE \
  --acfe-root /path/to/ACfE
```

Promote an already-built train payload without rebuilding:

```bash
python sdk-intake/build_ambiqsuite.py \
  --train r3 \
  --promote-only \
  --ambiqsuite-repo /path/to/ambiqSuite \
  --source-ref stable
```

## Update Steps Before A PR

1. Rebuild or re-promote the affected train(s).
1. Confirm provider payload diffs are intentional under `modules/nsx-ambiqsuite-r*/sdk/`.
1. Refresh provider metadata if the source snapshot changed.
1. Run the contract tests:

```bash
python -m pytest tests/test_cmake_contract.py -q
```

1. Review any train-specific policy changes, such as supported toolchains or board exposure.

## Notes

- Artifacts are release-style by default; the helper appends `-g0` after the
  native AmbiqSuite makefile flags. Use `--debug-symbols` only for local
  diagnostic builds.
- Single-toolchain reruns rebuild only that toolchain's native output and update
  only that toolchain's staged artifacts.
- `r2` intentionally does not support `acfe` / `armclang` because AmbiqSuite r2
  never shipped armclang startup/linker support.
