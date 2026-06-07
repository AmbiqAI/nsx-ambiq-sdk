# Contributing

This repository is the coordinated release unit for NSX AmbiqSuite R5 platform modules. Keep changes focused on the SDK-core substrate: provider payload, CMake descriptors, HAL/BSP adapters, startup integration, board descriptors, and NSX-owned runtime services.

## Local Checks

Install the test and development dependencies:

```sh
uv sync --group ci
uv run --group ci pre-commit install
```

Run the same checks as CI:

```sh
uv run --group ci pre-commit run --all-files
uv run --group ci python -m py_compile sdk-intake/build_ambiqsuite.py
uv run --group ci python -m pytest
```

## Development Rules

- Do not edit `modules/nsx-ambiqsuite-r5/sdk/` by hand. Regenerate the provider payload from `sdk-intake/build_ambiqsuite.py`.
- Keep HAL/BSP prebuilt library paths under `sdk/lib/<toolchain>/<part>/...`.
- Keep module source lists explicit in CMake. Do not use broad source globs.
- Keep toolchain, SoC, board, and application facts separated.
- Advertise only staged provider compatibility in `nsx-module.yaml` manifests.
- Keep optional middleware such as FreeRTOS, TinyUSB, Cordio, CMSIS-NN, and CMSIS-DSP outside this core bundle.

## Documentation Audience

- `README.md` is for users who need to understand what the repo provides and how to consume it.
- `CONTRIBUTING.md` is for developers changing this repo.
- `docs/` contains maintainer notes for platform coverage, SDK intake, and boundary decisions.

Prefer concise user-facing docs. Put rationale, survey notes, and future work in maintainer docs.
