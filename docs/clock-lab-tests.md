# Experimental clock and SysTick checks

This suite exercises the SDK power adapter on `apollo510b_evb` and
`apollo330mP_evb`. It is intentionally absent from qualification profiles while
the fitted reference crystal and startup/error budget in
[the clock contract](clock-timebase-qualification.md) remain unverified.
A passing diagnostic is not calibrated oscillator qualification.

## Local execution

Acquire the shared lab claim for the exact probe first. The runner adds a local
exclusive probe lease but does not replace the lab allocator. From this repo:

```sh
uv sync --group ci --group hardware
uv run pytest -q tests/hardware/test_clock.py --build-only --clock-experimental \
  --board apollo510b_evb --gcc-root /usr/local/arm-gnu-toolchain
uv run pytest -q tests/hardware/test_clock.py --hardware --clock-experimental \
  --board apollo510b_evb --probe 1160002954 \
  --gcc-root /usr/local/arm-gnu-toolchain --hardware-timeout 40 \
  --hardware-artifacts /tmp/nsx-clock-run
```

For AP330 Plus select `apollo330mP_evb` and its explicitly assigned serial.
Each build gets a fresh artifact directory. Build-only skips execution and is
not a hardware pass. Release the shared lab claim afterward. No microphone or
external audio is involved. This standalone pytest/API flow needs no GH runner.

## What is exercised

Unity runs reference initialization, the nominal transition matrix and final
LOW restoration. The matrix contains LOW→LOW, LOW→HIGH, HIGH→HIGH, HIGH→MAX,
LOW→MAX, MAX→MAX, MAX→HIGH, HIGH→LOW, MAX→LOW and NOT_SET from each API mode.
It repeats twice and captures three 4096-reference-tick windows per row.
AP510B HIGH/MAX are aliases; AP330 HIGH/MAX exercise HP1/HP2 separately.

Every row establishes its start, checks successful public mode selection,
`SystemCoreClock`, MCUPERFACK and actual MCUPERFSTATUS. AP510B additionally checks
HFRC2READY and inactive audio adjustment. AP330 configures a nominal 250 MHz PLL
with an explicit HFRC192DIV4 reference, then verifies lock, source and divider
ratio in HP2. The test never requests AP510B CPU operation at 196.608 MHz.

The [clock contract](clock-timebase-qualification.md) defines the reference
setup, per-window source checks, capture bounds, frequency budget and SysTick
accounting. The ±5.2% frequency screen assumes an unverified ±50 ppm assembled
reference; a pass remains conditional/experimental. Raw cycle/reference/interrupt
counts for all 72 windows are retained in the Unity log.

## Negative controls and cleanup

Repeat the hardware command with one `--fault-injection` value:

- `clock-stale-publication`: publishes 96 MHz after a high-speed transition;
  the exact nominal assertion must fail.
- `clock-no-reload`: leaves the starting-mode reload installed; measured
  SysTick counts must disagree after a frequency-changing transition.
- `clock-wrong-scaling`: doubles the reference conversion; the physical-rate
  envelope must fail.

These alter test firmware only. Expect a nonzero pytest exit and the intended
Unity assertion, not a transport timeout or unrelated failure. The worker saves
`last-result.bin` before decoding a failure. `restore_low` must still pass.
A normal, fault-free run follows the controls. Teardown disables SysTick, clears
its pending exception, restores interrupt masks and verifies LOW return/status.
Setup or cleanup failures invalidate the diagnostic.
