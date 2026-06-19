# Third-Party Payload Notes

`nsx-ambiqsuite-r5` is a curated AmbiqSuite R5.2.0 provider payload, not a full
SWS SDK mirror. Imported AmbiqSuite files retain their upstream headers and the
upstream license bundle is preserved under `sdk/docs/licenses/`.

## Bundled Notices

The payload carries the upstream AmbiqSuite license directory, including:

- AmbiqSuite software agreement: `sdk/docs/licenses/LICENSE.rtf`
- SEGGER RTT notice: `sdk/docs/licenses/SEGGER-RTT-license.txt`
- Think Silicon/Nema notice: `sdk/docs/licenses/ThinkSi-license.txt`
- R5-era third-party notices such as MBED TLS, OpenAMP, LibMetal, zlib, jQuery,
  Opus, CoreMark, Apache, BSD, and MIT notices carried from the SDK license
  bundle

## SEGGER

No SEGGER RTT, J-Link, or SystemView source or binary payload is intentionally
bundled in `sdk/`. The SEGGER file present in this provider is the upstream
license notice only.

Some NSX and Ambiq files mention J-Link, SWO, or SEGGER in comments or toolchain
configuration. Those are debugger/tool references, not bundled SEGGER software.

## Think Silicon / NemaGFX / NemaDC

The staged Ambiq display device headers reference Nema headers such as
`nema_dc.h` and `nema_sys_defs.h`.

This provider currently does not bundle the ThinkSi/Nema SDK header tree or
ThinkSi/Nema static libraries. Keep that boundary unless display/Nema APIs become
an explicit supported surface. If they do, import the minimal required ThinkSi
headers/configuration files from upstream and keep `ThinkSi-license.txt` with the
payload.

## Optional USB Module

TinyUSB is bundled under the optional `nsx-ambiq-usb-r5` module, not the core
`nsx-ambiqsuite-r5` provider payload. TinyUSB carries its own MIT license at
`modules/nsx-ambiq-usb-r5/sdk/third_party/tinyusb/source/LICENSE`.
