# nsx-ambiq-usb-r5

AmbiqSuite R5 USB substrate for TinyUSB on supported Ambiq SoCs.

This module stages the USB material that ships with the AmbiqSuite R5 SDK drop
and exposes it as an opt-in NSX module. It is intentionally separate from the
base `nsx-ambiqsuite-r5` SDK provider so applications that do not use USB do not
inherit TinyUSB or USB class-driver sources.

## Owns

- Ambiq TinyUSB device-controller port for R5-family SoCs.
- SDK-vendored TinyUSB sources needed by the Ambiq port.
- The patched Ambiq CDC/vendor class sources staged from the SDK drop.
- CMake target `nsx::ambiq_usb_r5` and compatibility alias `nsx::tinyusb`.

## Does Not Own

- USB descriptors, VID/PID, manufacturer/product strings, or WebUSB landing URLs.
- Which TinyUSB classes an application exposes beyond the wrapper's chosen config.
- RPC framing or application protocol behavior.
- Whether `tud_task()` is pumped by a bare-metal loop, an NSX timer helper, or an
  RTOS task.

Higher-level modules such as `nsx-usb` provide convenience APIs and default
bare-metal infrastructure. Applications and frameworks remain free to provide
their own descriptors and service model.

## Required Consumer Contract

The staged CDC class source expects the consuming wrapper or application to
provide these buffer hooks:

```c
uint8_t *nsx_usb_get_rx_buffer(void);
uint8_t *nsx_usb_get_tx_buffer(void);
uint32_t nsx_usb_get_cdc_rx_buffer_length(void);
uint32_t nsx_usb_get_cdc_tx_buffer_length(void);
```

Applications can either use the `nsx-usb` helper module, which provides these
hooks, or provide their own hook implementations and TinyUSB descriptor
callbacks for the selected classes.
