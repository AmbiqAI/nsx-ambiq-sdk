# nsx-interrupt

`nsx-interrupt` is the per-SDK interrupt-dispatch layer for AmbiqSuite-backed NSX targets.

It owns the raw startup vector symbols (such as the GPIO bank `am_gpio*_isr` handlers) and routes each NVIC IRQ line to a single registered handler. Higher-level wrappers (`nsx-gpio`, `nsx-uart`, `nsx-timer`) register a handler per IRQ line and layer their own semantics — pin/bank dispatch, RX/TX callbacks — on top.

Each line supports register, unregister, enable, disable, priority, and pending-clear. Handlers run in interrupt context; application scheduling and event routing remain with the caller.

Public interfaces live in `includes-api/`.
