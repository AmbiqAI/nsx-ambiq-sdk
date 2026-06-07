# nsx-ambiqsuite-r4

`nsx-ambiqsuite-r4` is the raw AmbiqSuite R4 SDK provider repo used by NSX.

This repo keeps the imported vendor SDK under `sdk/` and exposes a thin NSX
provider target through `CMakeLists.txt` and `nsx-module.yaml`.

The default branch tracks the latest validated R4 line for NSX. Older validated
R4 releases can live on versioned branches when needed.

## Toolchains

Prebuilt HAL/BSP libraries shipped under `sdk/lib/` are GCC builds; armclang
and ATfE consume the same prebuilts (Arm AAPCS-compatible). End-to-end builds
through this provider are validated under:

- `arm-none-eabi-gcc` (reference)
- `armclang` (Arm Compiler 6)
- `clang` / ATfE (Arm Toolchain for Embedded)

The vendored CMSIS-5 ARM core headers were removed from `sdk/CMSIS/ARM/Include/`;
CMSIS core headers are now provided exclusively by `nsx-cmsis-core` (CMSIS-6).
Apollo4 `system_*.h` headers remain staged under `sdk/CMSIS/AmbiqMicro/Include/`,
and the provider also stages the matching `system_apollo4l.c` and
`system_apollo4p.c` sources under `sdk/CMSIS/AmbiqMicro/Source/` so startup
consumes provider-owned CMSIS system sources just like the R3 and R5 tiers.

The promoted `sdk/devices/` payload is header-only. If SWS later approves
shipping device-driver source or standalone device archives, `sdk/devices/`
is the intended place to expose that expanded surface.

The promoted `sdk/utils/` payload is also header-only. Curated utility
implementations that NSX links today live under `sdk/src/`; additional
utility source or archives can be exposed later if SWS approves that surface.
Apollo4P audio and USB support files are intentionally not staged under
`sdk/src/apollo4p/`; those subsystem-specific pieces belong with `nsx-audio`
and `nsx-ambiq-usb-r4`, while the provider keeps the HAL headers and prebuilt
archives they consume.
Example-scaffolding residue such as `am_resources.c` and vendor example
`linker_script.ld` files are also intentionally omitted from the promoted
provider payload.
Standalone utility implementations that NSX does not currently build, such as
`am_util_id.c`, are also omitted until there is a concrete supported consumer
surface for them. Matching unsupported utility headers are omitted as well, so
the staged `utils/` surface does not advertise APIs that the provider payload
does not implement.
FreeRTOS kernel source is intentionally not staged in this provider payload.
If NSX needs a supported RTOS surface later, that should land in a dedicated
module rather than re-expanding `sdk/src/` here.

Promoted board payloads keep the BSP headers only. Toolchain project metadata
such as `iar/` and `keil6/` stays in the upstream source tree used to generate
the BSP archives and is not part of the promoted provider surface.
The same rule applies to staged MCU HAL headers: vendor IAR and Keil project
directories stay upstream and are not part of the promoted provider payload.

Promoted SDK payloads also omit legacy make metadata such as `module.mk`,
`board-defs.mk`, and vendor `Makefile` fragments. NSX consumes the staged
static libraries from `sdk/lib/` and the exported headers from the staged
include trees through CMake.

NSX curation for this provider must not patch imported AmbiqSuite source files
in place. Changes to the exposed surface should happen by selecting staged
files, omitting unsupported content, generating adjacent NSX-owned wrappers, or
adding NSX-owned integration files outside the imported vendor tree.

Licensing for the imported SDK content follows the upstream AmbiqSuite terms.
The root-level [`AM-BSD-EULA.txt`](AM-BSD-EULA.txt) is copied from the imported
R4.5.0 SDK snapshot for visibility.
