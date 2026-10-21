# Toolchain Link Smokes

This repo includes an opt-in link-smoke runner for validating that staged NSX
boards consume the expected HAL/BSP artifacts and can link a minimal executable.
The GCC PR-CI job builds these firmware smokes. ATfE and ACfE are optional in
ordinary CI; ACfE link steps require a licensed toolchain.

## Local ACfE Example

The smoke runner generates the standard CMake toolchain file automatically. To
inspect or reuse that file manually, write one with the helper:

```sh
uv run --group ci python tools/nsx_toolchain_file.py \
  --toolchain-family armclang \
  --output /tmp/acfe-toolchain.cmake
```

Run all staged boards:

```sh
export ACFE_ROOT=/Applications/ARMClangToolchain/6.24Rel19
export PATH="$ACFE_ROOT/bin:$PATH"

uv run --group ci python tools/nsx_link_smoke.py \
  --toolchain-family armclang \
  --build-root /tmp/nsx-link-smoke \
  --verbose
```

`tools/nsx_link_smoke.py` generates the standard toolchain file under the
build root when `--toolchain-file` is omitted. Pass `--toolchain-file` only when
validating a custom CMake toolchain definition.

The supported smoke matrix is the four staged M55 boards below, each with GCC,
ATfE and ArmClang. Apollo2/3/4, legacy Apollo5B and AT110 are not qualified by
this runner; their other tests and hardware acceptance remain separate.

```text
apollo330mP_evb
apollo510_evb
apollo510b_evb
apollo510dL_evb
```

Each successful run writes `smoke.elf` and `smoke-evidence.json` under
`<build-root>/<toolchain>/<board>/`. The record hashes the ELF, real startup,
System source, selected linker file and actual configured HAL/BSP inputs.

The firmware links real startup/System code, the core runtime used by Apollo510B
fault handlers, and required HAL/BSP functions. The ELF check requires the reset
entry, vector words, aligned RAM stack, SystemInit/main and vendor definitions,
and checks physical load bytes against the selected application NVM aperture.
Physical load spans must not overlap, vector bytes must load at the application
origin, and NVM execution addresses must match their physical load addresses.
GNU/ATfE data, shared-SRAM and TCM initializer loads must also match the
source symbols used by startup copying; destination symbols must cover the entire
initialized section and fit wholly within a declared RAM/TCM region, never NVM.
Required functions must have file-backed code sections.
Smoke images seed shared data and TCM
code so these paths are present in the linked image.
Both the ELF entry and reset vector must select Thumb Reset_Handler.
Reset_Handler must execute from application NVM before startup initializes RAM.
Runtime segment and allocated-section extents, including zero-fill data, must
fit the selected script's memory declarations. ITCM and scatter heap/stack
regions are included; adjacent regions are joined without accepting gaps.
The entry probe never substitutes clock/startup stubs. These are link/layout
checks; no board is flashed and peripheral behavior is not qualified.

ATfE requires its matching newlib overlay (`bin/newlib.cfg`); generated toolchain
files select it with `--config=newlib.cfg`, matching the NSX app runtime. The
container installs the version-matched official overlay and checks its published
SHA256. An installation without the overlay fails rather than silently testing
a different C runtime. See the [ATfE release assets](https://github.com/arm/arm-toolchain/releases/tag/release-22.1.0-ATfE).

`test_link_smoke.py` builds this matrix when compilers are installed and rejects
missing startup plus corrupt entries, vector/stack words, missing System/HAL
definitions, load overruns, overlaps, shifted load addresses, RAM reset targets
and runtime memory overruns. GCC is required in the cross-compile PR job;
ATfE/ArmClang remain local or optional container qualification. Skipped cases
are not passing evidence. ABI field-layout checks remain in `test_vendor_abi.py`.
ArmClang tests honor `ACFE_ROOT` before searching `PATH`; an invalid explicit
root fails the tests instead of skipping the compiler.

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

The ELF checks require every allocated, file-backed section to be fully covered
by a load segment, with matching file-offset/execution-address mappings for
GNU copy-down and execute-in-place sections. ArmClang scatter RAM/ITCM
initializers are checked within their declared execution regions and the
combined NVM file-backed load. Required function extents must fit executable
sections. Initial MSP must equal the actual startup stack top: the GNU
`g_pui32Stack` allocation or ArmClang's declared stack and matching linker
symbols. An arbitrary aligned RAM address is insufficient.
