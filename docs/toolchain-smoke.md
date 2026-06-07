# Toolchain Link Smokes

This repo includes an opt-in link-smoke runner for validating that staged NSX
boards consume the expected HAL/BSP artifacts and can link a minimal executable.
Normal CI does not run these smokes because they build firmware-like link
targets and ACfE link steps require license configuration.

## Local ACfE Example

The smoke runner generates the standard CMake toolchain file automatically. To
inspect or reuse that file manually, write one with the helper:

```sh
uv run --group ci python tools/nsx_r5_toolchain_file.py \
  --toolchain-family armclang \
  --output /tmp/acfe-toolchain.cmake
```

Run all staged boards:

```sh
export ACFE_ROOT=/Applications/ARMClangToolchain/6.24Rel19
export PATH="$ACFE_ROOT/bin:$PATH"

uv run --group ci python tools/nsx_r5_link_smoke.py \
  --toolchain-family armclang \
  --build-root /tmp/nsx-r5-link-smoke \
  --verbose
```

`tools/nsx_r5_link_smoke.py` generates the standard toolchain file under the
build root when `--toolchain-file` is omitted. Pass `--toolchain-file` only when
validating a custom CMake toolchain definition.

Expected outputs:

```text
/tmp/nsx-r5-link-smoke/armclang/apollo330mP_evb/smoke.elf
/tmp/nsx-r5-link-smoke/armclang/apollo510_evb/smoke.elf
/tmp/nsx-r5-link-smoke/armclang/apollo510b_evb/smoke.elf
/tmp/nsx-r5-link-smoke/armclang/apollo510dL_evb/smoke.elf
```

## Standard Dev/CI Container

`docker/dev-ci.Dockerfile` installs shared build prerequisites, uv, CMake,
Ninja, Make, Arm GNU Toolchain, Arm Toolchain for Embedded, and Arm Compiler for
Embedded. `.devcontainer/devcontainer.json` uses the same Dockerfile for local
VS Code Dev Containers, and GitHub Actions can use it for additional validation
jobs as they are added.

Pinned defaults:

```text
ARM_GNU_VERSION=14.3.rel1
ATFE_VERSION=22.1.0
ATFE_RELEASE_TAG=release-22.1.0-ATfE
ARM_COMPILER_VERSION=6.24
ARM_COMPILER_REL=19
GCC_ROOT=/opt/toolchains/gcc
ATFE_ROOT=/opt/toolchains/atfe
ACFE_ROOT=/opt/toolchains/acfe
```

Arm GNU Toolchain is downloaded from Arm's `14.3.rel1` binary release and
verified against the published MD5 values used by TensorFlow Lite Micro helper
scripts. ATfE is downloaded from the `arm/arm-toolchain` GitHub release assets
and verified with the published sha256 file. ACfE is downloaded from Arm's
compiler artifact URL using the same standalone archive naming used by
TensorFlow Lite Micro helper scripts.

`.github/workflows/container-validation.yml` is manual-only and disabled by
default. When explicitly dispatched, it builds the local Dockerfile and runs the
selected GCC, ATfE, or ACfE smoke. ACfE runs require `ARMLM_ACTIVATION_CODE` to
be configured as a repository secret. Keep the normal `CI` workflow free of ACfE
license requirements.

For local devcontainer use, export `ARMLM_ACTIVATION_CODE` in the shell that
runs `devcontainer up`. `.devcontainer/devcontainer.json` passes the value
through without storing it in the repo or image, and `.devcontainer/post-create.sh`
activates the license for the root user with `armlm activate -code ...`.
