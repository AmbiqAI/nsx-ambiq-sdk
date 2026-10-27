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
until setup completes and throughout teardown. Setup preserves the caller’s
interrupt mask and defers IRQ delivery until readiness is published. An owned timer interrupt is
acknowledged even while its callback is suppressed. Call init/deinit from foreground;
concurrent lifecycle calls and deinitializing from a callback are unsupported.

Deinitialization unregisters the IRQ, disables the timer interrupt, stops the
counter and clears pending interrupts. A failure retains ownership for a later
deinit retry. Other families return `NSX_STATUS_INVALID_CONFIG` from deinit;
their existing initialization behavior is unchanged.

## Apollo4/510/330 period contract

These adapters explicitly select the nominal 6 MHz HFRC/16 timer clock.
Interrupt periods must be 1 through 715827882 microseconds, inclusive; zero or
larger values return `NSX_STATUS_INVALID_CONFIG` before changing the timer or
its owner, even when optional API validation is disabled. Integer microseconds
convert exactly to six ticks each. Counter reads truncate fractional microseconds.
The period field is ignored for counter-only operation.

The ISR acknowledges compare 1 and clears the counter before the callback.
Interrupt latency therefore extends the callback interval; masking interrupts
or a long callback can delay delivery. This is a lightweight periodic callback,
not phase-locked scheduling. The clock is independent of the CPU performance
setting, but its absolute accuracy follows HFRC. AP3 uses a different STIMER
backend; this contract does not qualify AT110's distinct clock tree.
