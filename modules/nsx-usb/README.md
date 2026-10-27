# nsx-usb

USB CDC serial driver for Ambiq SoCs, backed by the SDK-hosted TinyUSB substrate.

This module keeps the NSX USB interface in the SDK repo so SDK updates stay
localized to one project. It selects the staged TinyUSB substrate that matches
the active AmbiqSuite provider tier.

## Scope

- Exposes the public NSX USB API through `nsx_usb.h`
- Provides CDC and optional vendor/WebUSB helpers
- Builds descriptor and TinyUSB callback overrides into the active SDK USB substrate
- Preserves the `nsx::usb` target and `nsx-usb` module contract for consumers

## Supported SoCs

Apollo4P, Apollo330P, Apollo510, Apollo510B, Apollo510L.

## Connection and transfer behavior

`nsx_usb_connected()` reports configured, nonsuspended USB with CDC DTR asserted.
`nsx_usb_vendor_connected()` requires configured, nonsuspended USB plus the
vendor logical session flag; it does not require CDC DTR. The vendor control request or a higher-level consumer such as TileIO can
open that session. Mount, NSX-owned unmount and suspend clear it; resume alone
does not reopen it. AP4 retains its DCD-owned unmount teardown; USB readiness
prevents a stale session flag from reporting connected while unconfigured.
Both queries are read-only snapshots and return false for invalid/uninitialized
configurations. They do not guarantee the next transfer will succeed.

Low-level CDC/vendor transfers preserve buffered operation without imposing a
logical-connection check. Applications needing that policy must check it before
transferring; it remains possible to disconnect after the check. Vendor write
availability services USB events before checking readiness and logical state.

| Operation | Result contract |
| --- | --- |
| CDC/vendor send | SUCCESS when all bytes are accepted into the transport; TIMEOUT may include partial `bytes_sent`. Acceptance is not host acknowledgement. |
| Blocking CDC receive | SUCCESS for the requested count, PARTIAL for some data, TIMEOUT for none. Inspect `bytes_received`. |
| Nonblocking CDC/vendor read | SUCCESS may return zero bytes when no data is buffered. |

`NSX_USB_STATUS_NOT_CONNECTED` remains available to higher-level consumers;
these low-level transfers do not return it. Timeout accounting is cumulative
no-progress waiting, not a wall-clock deadline. Host software should explicitly
control CDC DTR when its connection policy depends on it; pyserial's `dsrdtr`
option controls flow control rather than the DTR signal value.
