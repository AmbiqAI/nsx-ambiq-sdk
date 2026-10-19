# Selected vendor archive ABI

Standalone SDK CMake consumers must compile vendor-facing types with the ABI of
the selected HAL/BSP archives. Artifact selection, not compiler branding, decides
these requirements:

| Selected directory | Enum layout | wchar_t |
| --- | --- | --- |
| gcc | short | 32-bit |
| atfe | int | 32-bit |
| acfe | short | 16-bit |

These values follow the recorded object attributes and real HAL structure probes.
The current ATfE workaround selects GCC archives, so its default consumer also
uses short enums. Hard-float remains required. C and C++ usage requirements are
carried by the SoC flags target; assembler inputs do not receive enum/wchar flags.

Set an explicit NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN before including the board/SoC
descriptor. Changing it after ABI target creation is rejected. Custom library
path overrides must obey the declared selected artifact ABI; they are not
automatically reclassified by arbitrary archive contents. Do not append
conflicting per-source options or pragmas to vendor-facing translation units.

Generated neuralspotx apps already set short-enum flags for their normal SDK
path; this repair closes the standalone SDK interface gap without changing
vendor archives. The focused tests compile real PDM layouts in C/C++ for all
three compilers, verify conflicting global defaults are overridden correctly,
and force the selected HAL implementation into a relocatable link with ABI
warnings fatal. This checks layout/link compatibility, not peripheral behavior.
