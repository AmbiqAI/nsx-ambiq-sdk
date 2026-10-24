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

STIMER is explicitly cleared and configured to the 32 kHz crystal, while DWT
counts CPU cycles. A half-second reference pre-roll is a warm diagnostic policy,
not proof of cold-start stabilization. A bounded polling loop, a 300-million-
cycle acquisition ceiling, and the host deadline prevent an unbounded run.
Snapshots mask interrupts briefly and bracket the STIMER read with DWT reads;
a snapshot exceeding 2048 cycles is rejected. The firmware enables no interrupt
source except SysTick. The runner never halts the target during a window, but
reads its completion word through SWD. Transport/bus effects still need a formal
error-budget review. These constraints do not cover arbitrary debugger halts.

The provisional physical-rate envelope is ±5.2% around nominal. Its inputs are
the documented ±5% HFRC/HFRC2 accuracy, an **unverified EVB assumption** that the
32.768 kHz reference meets the datasheet's ±50 ppm requirement, and rounding
headroom for endpoint uncertainty. At the slowest supported rate (96 MHz minus
5%), two 2048-cycle endpoint apertures contribute less than 0.036% over 125 ms;
two reference-tick quantization intervals contribute less than 0.049%. Combining
these with reference error fits within the additional 0.2%. AP330's locked PLL
inherits its HFRC reference accuracy. These are proposed regression limits,
not a completed fitted-component/temperature/aging or PLL jitter budget. Keep
`--clock-experimental` and do not add a qualification profile until review of
those prerequisites is complete. Do not tune bounds to measured board results.

The application reloads SysTick after each mode change. Measured interrupt count
is checked against independently captured CPU cycles and the **expected** reload,
allowing two boundary interrupts plus snapshot aperture. This proves reload and
interrupt accounting; it is not an independent ppm measurement of SysTick's
clock source. The separate crystal-referenced CPU check establishes its physical
rate under the provisional assumptions above. Raw cycle/reference/interrupt
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
