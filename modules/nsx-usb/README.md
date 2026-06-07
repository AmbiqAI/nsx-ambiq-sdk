# nsx-usb

USB CDC serial driver for Ambiq SoCs, backed by the SDK-hosted TinyUSB substrate.

This module keeps the NSX USB interface in the AmbiqSuite R5 SDK repo so SDK
updates stay localized to one project.

## Scope

- Exposes the public NSX USB API through `nsx_usb.h`
- Provides CDC and optional vendor/WebUSB helpers
- Builds descriptor and TinyUSB callback overrides into the R5 USB substrate
- Preserves the `nsx::usb` target and `nsx-usb` module contract for consumers

## Supported SoCs

Apollo330P, Apollo510, Apollo510B, Apollo510L.
