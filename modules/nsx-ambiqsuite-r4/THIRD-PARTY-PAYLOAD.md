# Third-Party Payload Notes

`nsx-ambiqsuite-r4` is a curated AmbiqSuite R4.5.0 provider payload, not a full
SWS SDK mirror. Imported AmbiqSuite files retain their upstream headers and the
upstream license bundle is preserved under `sdk/docs/licenses/`.

## Bundled Notices

The payload carries the upstream AmbiqSuite license directory, including:

- AmbiqSuite software agreement: `sdk/docs/licenses/LICENSE.rtf`
- FreeRTOS/MIT notices: MIT files under `sdk/docs/licenses/`; the provider does
  not stage the upstream FreeRTOS kernel source subset
- SEGGER RTT notice: `sdk/docs/licenses/SEGGER-RTT-license.txt`
- Think Silicon/Nema notice: `sdk/docs/licenses/ThinkSi-license.txt`
- Additional upstream notices such as Opus and GPL text carried from the SDK
  license bundle

## SEGGER

No SEGGER RTT, J-Link, or SystemView source or binary payload is intentionally
bundled in `sdk/`. The SEGGER file present in this provider is the upstream
license notice only.

Some Ambiq board pin descriptions and debugger project metadata mention J-Link
or SEGGER. Those are configuration references, not bundled SEGGER software.

## Think Silicon / NemaGFX / NemaDC

The staged Ambiq display device headers reference Nema headers such as
`nema_dc.h` and `nema_sys_defs.h`, and the staged HAL/BSP archives were built
against ThinkSi include paths from the upstream SWS SDK source tree.

This provider currently does not bundle the ThinkSi/Nema SDK header tree or
ThinkSi/Nema static libraries. Keep that boundary unless display/Nema APIs become
an explicit supported surface. If they do, import the minimal required ThinkSi
headers/configuration files from upstream and keep `ThinkSi-license.txt` with the
payload.

## Other Third-Party Content

CMSIS Core headers are intentionally not bundled from AmbiqSuite R4. NSX uses
the separate `nsx-cmsis-core` module, which packages ARM CMSIS_6 v6.2.0
CMSIS-Core(M) headers. The AmbiqSuite provider keeps only AmbiqMicro
device/system headers under `sdk/CMSIS/AmbiqMicro/Include`.

CMSIS-DSP and CMSIS-NN are intentionally not bundled in this provider. NSX
publishes those libraries separately, so the R4 AmbiqSuite provider must not
stage AmbiqSuite CMSIS Core 5 headers, `arm_math*`, `dsp/`, `arm_nn*`, or
`libarm_cortex*M4*math*` payloads.

FreeRTOS kernel source is also intentionally not bundled in this provider
payload. If NSX later needs a supported FreeRTOS surface, stage it through a
dedicated RTOS module instead of reintroducing those files into `sdk/src/`.
