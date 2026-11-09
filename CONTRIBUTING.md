# Contributing

This repository is the coordinated release unit for NSX AmbiqSuite-backed platform modules. Keep changes focused on the SDK-core substrate: provider payload, CMake descriptors, HAL/BSP adapters, startup integration, board descriptors, and NSX-owned runtime services.

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

## Required CI checks

The main-branch ruleset is maintained in GitHub. Its required check contexts are
`Python 3.10`, `Python 3.12`, and
`nsx-npu cross-compile (arm-none-eabi-gcc)`, all from GitHub Actions. The ARM job
covers SDK integration fixtures as well as NPU sources. Keep these jobs running
on every PR, including release PRs, and coordinate any context rename or matrix
change with the ruleset. Manual container validation and Copilot reviews are
separate from the required status checks.

PR branches must be up to date with `main` before merging. After updating a
branch, wait for the required checks on the updated revision. A missing check
is not a pass; inspect the workflow trigger and run before retrying. SDK tag
creation and publication remain separate approved steps.

## Development Rules

- Do not edit `modules/nsx-ambiqsuite/sdk/` by hand. Regenerate the provider payload from `sdk-intake/build_ambiqsuite.py`.
- Prefer the staged workflow in `sdk-intake/intake_workflow.py` (see [`docs/intake-hardening.md`](docs/intake-hardening.md)) when reviewing a payload change before promoting it.
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

## Physical board tests

See [local hardware tests](docs/local-hardware-tests.md) for the opt-in
pytest/Unity startup harness, local commands, hardware ownership requirements,
and evidence limitations. No GitHub runner is required.

## Refresh Python tooling and container trust

The dev/CI image pins uv 0.13.6; the CI dependency group requires virtualenv
21.14.12 or newer so fresh pre-commit environments seed a corrected pip trust
bundle. See the [Python maintainer notice](https://discuss.python.org/t/information-for-python-users-regarding-emudhra-emsign-ca-certificate-security-incident/109576/1)
for the emSign incident and current recommendations.

Rebuild existing images with fresh base/system certificate layers:

```sh
docker build --pull --no-cache -f docker/dev-ci.Dockerfile -t nsx-dev-ci:local .
```

For a smaller package-tooling check using the same base as the full image:

```sh
docker build --pull --no-cache --target tooling -f docker/dev-ci.Dockerfile -t nsx-python-tooling:local .
docker run --rm -v "$PWD:/workspace:ro" -w /workspace nsx-python-tooling:local bash tools/validate_python_tooling.sh
```

The manual Container Validation workflow always runs this tooling check; full
compiler/image validation remains opt-in. The check installs locked dependencies
and validates a fresh virtualenv's pip/certifi versions and TLS-enabled download.

Rebuilding an image does not repair retained virtual environments or pre-commit
caches. Recreate affected project environments, run `uv sync --locked --group ci`,
and use `uv run --group ci pre-commit clean` before recreating hook environments.
Coordinate cache cleanup with other running work. Review other installed Python
tools against the notice; this repository change does not update host-wide tools,
existing containers, or deployed images. Never disable certificate verification.
