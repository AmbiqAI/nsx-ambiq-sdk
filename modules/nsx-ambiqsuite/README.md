# nsx-ambiqsuite

`nsx-ambiqsuite` is the unified AmbiqSuite SDK provider for NSX modules.

This module exposes the curated AmbiqSuite SDK provider payload under `sdk/`
and a thin NSX provider target through `CMakeLists.txt` and `nsx-module.yaml`.

## SDK Version

`sdk/artifact-manifest.yaml` records the `stable-2026.11.05`
intake: upstream `3cfb7d50718e6fb016f1a0128121cb40598109d5`, prepared source
`6a2a0c08af553e33716f2e251cb74aa370e2ae0f`. The latter includes the AT110 XSPI
capacity/alignment source patches, applied before all 73 HAL/BSP archives were
built. `source_patch_provenance` records the original source and patch hashes.
The curated payload carries four patches under `sdk-intake/patches/stable/`:
compiler detection, unnamed-bitfield qualifiers, C++17 CC312 compatibility,
and EM9305 full-duplex receive scratch capacity.

The coordinated distribution version is `5.2.29` (release candidate).
The matching EM9305 transport fixes and bounded GCC hardware qualification
are recorded in `release/qualification-5.2.29.md` at the repository root.
This is not an official numbered AmbiqSuite release.

## Package Contents

| Directory | Contents |
|-----------|----------|
| `sdk/CMSIS/` | AmbiqMicro device headers plus curated per-family `system_*.c` sources |
| `sdk/boards/` | Promoted BSP headers for staged boards (no BSP source) |
| `sdk/devices/` | Approved device-driver headers and source |
| `sdk/lib/` | Toolchain-first prebuilt `libam_hal.a`, `libam_bsp.a` for GCC, ATfE, and ACfE |
| `sdk/mcu/` | HAL headers + register definitions (no source) |
| `sdk/src/` | Curated utility sources (`am_util_stdio.c`, `am_util_delay.c`, `am_util_pmu.c`) |
| `sdk/utils/` | Utility headers only (no source) |

HAL and BSP are provided as prebuilt static libraries. The provider payload
contains the approved headers, CMSIS system sources, utility sources, and libraries
listed above.

`sdk/utils/` follows the same pattern: the promoted payload keeps utility
headers in `sdk/utils/`, while the utility implementations NSX links today live
in `sdk/src/`. Per-family `system_*.c` startup sources stay in
`sdk/CMSIS/AmbiqMicro/Source/`, matching the upstream CMSIS ownership boundary.
Additional utility source or archives can be exposed later if SWS approves that
surface.
Unsupported utility headers that are not backed by the staged sources or
prebuilt libraries, such as `am_util_id.h`, are omitted so the promoted
`sdk/utils/` surface only advertises supported APIs.

Prebuilts under `sdk/lib/` carry `gcc`, `atfe`, and `acfe` artifact families.
NSX maps the `armclang` toolchain family to the `acfe` artifact directory.
For HAL/BSP linking today, NSX also maps the `atfe` toolchain family to the
`gcc` prebuilts as a temporary runtime workaround for the Apollo510 USB CDC
regression; the staged `atfe` archives remain in the provider payload for intake
and validation but are not the default consumer link target yet.

CMSIS core headers are provided by `nsx-cmsis-core` (CMSIS 6). CMSIS NN and
CMSIS DSP are separate module inputs.

## Source Boundary

Provider promotion is destructive and repeatable: `sdk-intake/build_ambiqsuite.py
--promote-only` recreates `sdk/` from the local AmbiqSuite drop and generated
artifacts. The V1 source payload is the three utility sources linked by
`nsx-ambiq-hal`, plus the CMSIS per-family `system_*.c` sources staged under
`sdk/CMSIS/AmbiqMicro/Source/`.

NSX curation for this provider must not patch imported AmbiqSuite source files
in place. Changes to the exposed surface should happen by selecting staged
files, omitting unsupported content, generating adjacent NSX-owned wrappers, or
adding NSX-owned integration files outside the imported vendor tree.

Licensing for the imported SDK content follows the upstream AmbiqSuite terms.
See [`UPSTREAM-LICENSE-NOTICE.md`](UPSTREAM-LICENSE-NOTICE.md) for the
provenance note for this repo.

### AT110 peripheral runtime

The AT110 HAL archives include AmbiqSuite's bare-metal CMSIS-RTOS interface
implementation. Its matching `cmsis_os2.h` and `cmsis_types.h` ship under
`mcu/atomiq110/hal/cmsis_os2` and are exposed only by that MCU's HAL target.
These control-block layouts are part of the prebuilt HAL ABI; substituting
another RTOS port's definitions is not supported. This does not qualify the
bare-metal implementation as a scheduler or as general interrupt-safe locking.
