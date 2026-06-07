# nsx-uart

`nsx-uart` provides the NSX wrapper for Ambiq UART usage across the
supported Apollo families.

Purpose:
- expose the `nsx_uart_*` API surface
- normalize family-specific UART setup behind one module
- provide an NSX-managed UART layer for app and service modules

Dependencies:
- `nsx-core`
- `nsx-core`
- `nsx-timer`
