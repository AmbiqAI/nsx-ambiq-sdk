# nsx-timer

`nsx-timer` provides caller-directed timer, ticker, and periodic callback helpers for AmbiqSuite-backed NSX targets.

It owns hardware timer setup and ISR callback routing. Application scheduling, task loops, and semantic timer roles remain with the caller or board/application layer.

Public interfaces live in `includes-api/`.

## Apollo4 lifecycle

Apollo4 timer initialization reserves the selected timer until
`nsx_timer_deinit()` succeeds, including when initialization fails. A second
initialization of a reserved timer is rejected; release it before reconfiguring.
Keep the original configuration object alive and unchanged until release.
Initialization checks each required HAL operation, and callbacks are suppressed
until setup completes and throughout teardown. Call init/deinit from foreground;
concurrent lifecycle calls and deinitializing from a callback are unsupported.

Deinitialization unregisters the IRQ, disables the timer interrupt, stops the
counter and clears pending interrupts. A failure retains ownership for a later
deinit retry. Other families return `NSX_STATUS_INVALID_CONFIG` from deinit;
their existing initialization behavior is unchanged.
