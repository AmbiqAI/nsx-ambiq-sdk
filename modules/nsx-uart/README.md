# nsx-uart

`nsx-uart` provides the NSX wrapper for Ambiq UART usage across the
supported Apollo families.

It owns the shared `nsx_uart_*` API surface and normalizes family-specific UART
setup, ISR routing, and callback registration behind one module. Higher-level
protocol framing, buffering policy, and application transport semantics remain
with the caller.

Dependencies:
- `nsx-core`
- `nsx-interrupt`
- `nsx-soc-hal`

Public interfaces live in `includes-api/`.
