# nsx-ambiqsuite

`nsx-ambiqsuite` is the unified AmbiqSuite SDK provider for NSX modules.

This module exposes the curated AmbiqSuite SDK provider payload under `sdk/`
and a thin NSX provider target through `CMakeLists.txt` and `nsx-module.yaml`.

## SDK Version

**AmbiqSuite snapshot `stable-2026.06.18`**, source ref `stable`, source commit
`caaf5af86087881647f56c70646c748d40c86e23`.

This is the exact promoted source identity recorded by
`sdk/artifact-manifest.yaml`; it is not represented as an official numbered
AmbiqSuite release. The NSX distribution and module version is `5.2.25`.

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
