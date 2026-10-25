# nsx-timer

`nsx-timer` provides caller-directed timer, ticker, and periodic callback helpers for AmbiqSuite-backed NSX targets.

It owns hardware timer setup and ISR callback routing. Application scheduling, task loops, and semantic timer roles remain with the caller or board/application layer.

Public interfaces live in `includes-api/`.

## Apollo4 lifecycle

Apollo4 timer initialization reserves the selected timer until
`nsx_timer_deinit()` succeeds, including when initialization fails. A different configuration object receives `NSX_TIMER_STATUS_IN_USE` without
claiming or changing the reserved timer. Reinitializing
with the same object releases the previous setup first, preserving callers such
as USB that update the period before reinitializing. Cleanup uses saved IRQ
settings, so switching to non-interrupt operation releases the previous handler.
Keep the original configuration object alive and its timer number unchanged
until release.
Initialization checks each required HAL operation, and callbacks are suppressed
until setup completes and throughout teardown. An owned timer interrupt is
acknowledged even while its callback is suppressed. Call init/deinit from foreground;
concurrent lifecycle calls and deinitializing from a callback are unsupported.

Deinitialization unregisters the IRQ, disables the timer interrupt, stops the
counter and clears pending interrupts. A failure retains ownership for a later
deinit retry. Other families return `NSX_STATUS_INVALID_CONFIG` from deinit;
their existing initialization behavior is unchanged.
