# SDK Intake Helpers

This folder contains source-controlled helpers for turning a local AmbiqSuite SDK
zip drop into the curated HAL/BSP artifact layout used by this repo.

Raw SDK zips and extracted source stay under the single ignored local intake
workspace:

```text
sdk-intake/local/drops/
sdk-intake/local/work/
```

Built, approved artifacts are copied into source-controlled release payloads:

```text
artifacts/ambiqsuite/<version>/<toolchain>/lib/...
artifacts/ambiqsuite/<version>/manifest.yaml
```

## R5.2.0 Build

Place the SWS zip at the default location:

```text
sdk-intake/local/drops/AmbiqSuite_R5.2.0.zip
```

Build GCC artifacts:

```bash
python sdk-intake/build_ambiqsuite.py --version R5.2.0 --toolchain gcc
```

Build ATfE artifacts:

```bash
python sdk-intake/build_ambiqsuite.py --version R5.2.0 --toolchain atfe \
  --atfe-root /path/to/ATfE-22.1.0
```

Build ACfE artifacts on a machine with an Arm Compiler for Embedded license:

```bash
export ARMLM_ACTIVATION_CODE=<activation-code>
armlm activate -code "${ARMLM_ACTIVATION_CODE}"
python sdk-intake/build_ambiqsuite.py --version R5.2.0 --toolchain acfe \
  --acfe-root /path/to/ACfE
```

Artifacts are release-style by default. The helper appends `-g0` after the
native AmbiqSuite makefile flags so the generated static libraries do not carry
compiler debug sections. Use `--debug-symbols` only for a local diagnostic build
that should not be promoted as a release artifact.

The helper is intentionally rerunnable for one toolchain at a time. It removes
that toolchain's native build output directories before rebuilding, copies only
that toolchain's libraries into `artifacts/`, and regenerates the manifest from
the artifacts currently present.

## Scope

The helper drives AmbiqSuite's native HAL/BSP makefiles. It does not import raw
SDK source into the curated module payload, and it does not build examples,
FreeRTOS, TinyUSB, Cordio, CMSIS-NN, or CMSIS-DSP.
