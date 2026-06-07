# nsx-ambiqsuite-r3

`nsx-ambiqsuite-r3` is the raw AmbiqSuite R3 SDK provider repo used by NSX.

This repo keeps the imported vendor SDK under `sdk/` and exposes a thin NSX
provider target through `CMakeLists.txt` and `nsx-module.yaml`.

The default branch tracks the latest validated R3 line for NSX. Older validated
R3 releases can live on versioned branches when needed.

## Toolchains

Prebuilt HAL/BSP libraries shipped under `sdk/lib/` are GCC builds; armclang
and ATfE consume the same prebuilts (Arm AAPCS-compatible). End-to-end builds
through this provider are validated under:

- `arm-none-eabi-gcc` (reference)
- `armclang` (Arm Compiler 6)
- `clang` / ATfE (Arm Toolchain for Embedded)

The vendored CMSIS-5 ARM core headers were removed from `sdk/CMSIS/ARM/Include/`;
CMSIS core headers are now provided exclusively by `nsx-cmsis-core` (CMSIS-6).

The promoted `sdk/devices/` payload is header-only. If SWS later approves
shipping device-driver source or standalone device archives, `sdk/devices/`
is the intended place to expose that expanded surface.

The promoted `sdk/utils/` payload is also header-only. Curated utility
implementations that NSX links today live under `sdk/src/`; additional
utility source or archives can be exposed later if SWS approves that surface.
Unsupported utility headers that are not backed by the staged sources or
prebuilt libraries, such as `am_util_id.h`, are omitted so the promoted
`sdk/utils/` surface only advertises supported APIs.
AmbiqMicro CMSIS system sources stay under `sdk/CMSIS/AmbiqMicro/Source/`, and
the startup module consumes those staged `system_apollo3*.c` files directly
rather than duplicating them elsewhere in the provider payload.

Promoted SDK payloads omit legacy make metadata such as `module.mk`,
`board-defs.mk`, and vendor `Makefile` fragments. NSX consumes the staged
static libraries from `sdk/lib/` and the exported headers from the staged
include trees through CMake.

NSX curation for this provider must not patch imported AmbiqSuite source files
in place. Changes to the exposed surface should happen by selecting staged
files, omitting unsupported content, generating adjacent NSX-owned wrappers, or
adding NSX-owned integration files outside the imported vendor tree.

Licensing for the imported SDK content follows the upstream AmbiqSuite terms.
The root-level [`AM-BSD-EULA.txt`](AM-BSD-EULA.txt) is copied from the imported
R3.2.0 SDK snapshot for visibility.
