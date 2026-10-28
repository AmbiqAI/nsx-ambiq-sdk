# CPU frequency synchronization in nsx-power

`nsx_power_set_performance_mode()` and `nsx_set_perf_mode()` share family selectors for AP3/3P, AP4 and
Apollo510/B/AP330P/510L operation in `modules/nsx-core/src/nsx_performance.c`.
The power adapter explicitly translates its legacy enum. Successful selection
updates CMSIS `SystemCoreClock` after verifying hardware ACK/mode; callers own
SysTick/RTOS reloads, cached conversions and serialized clock changes. The helper
does not calibrate oscillators. Atomiq110 retains its existing selector;
AP4 FPGA retains its existing lack of clock publication.

| Part | LOW | HIGH | MAX |
| --- | --- | --- | --- |
| Apollo3/3P silicon | Normal 24/48 MHz (preserves divider) | Burst 96 MHz | Same as HIGH |
| Apollo4P/L silicon | Nominal 96 MHz | Nominal 192 MHz | Same as HIGH |
| Apollo510 / Apollo510B | HAL LP constant (nominal 96 MHz) | Nominal 250 MHz; rejected during 196.608 MHz audio adjustment | Same as HIGH |
| Apollo330P / Apollo510L | HAL LP constant (nominal 96 MHz) | HAL HP1 constant (nominal 192 MHz) | Configured PLLVCO rate, 192–250 MHz on silicon |

On silicon, the HFRC2 or HP2 PLLVCO query runs before the mode switch. A
failed query or an invalid rate returns an error before changing the CPU
mode: Apollo510 accepts the HAL's free-running 250 MHz sentinel (zero),
adjusted 250 MHz, or a pending inactive 196.608 MHz setting; Apollo330P/510L require 192–250 MHz
for HP2. The free-running sentinel is normalized to nominal hertz.
Apollo510 also checks the HF2ADJ enable register: clock-manager configuration
can be deferred until a peripheral requests HFRC2, and entering HP does not
apply that pending adjustment. An inactive adjustment uses nominal free-running
250 MHz even if a different rate has been stored for later use. FPGA builds publish the scaled HAL mode constants
instead: their clock-manager configuration retains nominal silicon rates,
which are not the FPGA execution rates. A failed HAL switch propagates its status and does not publish a new
frequency. As with the HAL itself, a failed hardware transition is not a
guarantee that the hardware remains in a known state; callers must handle the
error rather than continuing on an assumed frequency.

## Apollo3 normal/burst contract

System LOW and power LOW select normal mode; system MEDIUM/HIGH and power
HIGH/MAX select burst. This changes the former system-selector no-op: an
AP3 startup configuration requesting HIGH now requests burst and can fail.
LOW works without burst availability and does not initialize burst support.
Burst requires CORESEL=0, SKU permission, enabled BLE buck, and (AP3P) matching core
supply request/status. Full BSP startup normally prepares supplies; minimal
startup only configures cache/FPU, so custom startup owns power preparation.
No implicit BSP initialization, divider change or startup charge is performed.

The HAL owns the patch-dependent power sequence and SRAM-based burst-disable
workaround. Unpatched AP3P retains the vendor fallback; this does not qualify
it against the underlying vendor MSPI/power erratum. Repeated confirmed modes
do not replay transitions. Successful initialization is shared between both APIs;
failed initialization is retried. FPGA selection is explicitly unsupported.
Failure retains the prior clock publication, without promising hardware rollback.

`tests/test_ap3_performance.py` covers unavailable SKUs, preparation/divider
rejection, missing HAL outputs, status errors, hardware mismatch, and mixed API
calls. `ap3-performance` is a separate opt-in board suite: it compares DWT cycles
with STIMER HFRC/16 across 24/48/96 MHz. Both share HFRC, so this checks relative
clock behavior, not absolute oscillator accuracy or silicon erratum coverage.

## Apollo510 CPU/audio clock compatibility

Apollo510/B documents 96 MHz LP and 250 MHz HP CPU modes. HFRC2 can be
adjusted to 196.608 MHz for audio, but that does not establish a supported
196.608 MHz HP CPU operating point. The performance selector rejects HP/MAX
while that adjustment is enabled, without switching mode or publishing a new
`SystemCoreClock`. LP remains available. The enable bit is used as a conservative
conflict check, not as evidence of frequency lock.

Adjusted-HFRC2 audio init and start each require a successful HAL query showing
LP CPU mode. Checking start also catches LP init followed by a switch to HP.
A rejected init/start does not configure or enable the audio clock; the caller
can select LP and retry. After audio stops and adjustment is disabled, HP is
available again. Plain-HFRC audio does not queue an unrelated HFRC2 adjustment.

Serialize performance changes with audio lifecycle calls. Direct HAL clock or
power calls bypass these guards. Other consumers that retune HFRC2 also need
compatible ownership; this is not a general shared-clock arbiter. PLL audio is
not qualified by these guards: the AP510B test currently times out waiting for
PLL lock. No automatic CPU downshift or alternate-source fallback is performed.

## Evidence and conflicting sources

- The vendored per-part `hal/mcu/am_hal_clkgen.h` headers define
  `AM_HAL_CLKGEN_FREQ_MAX_HZ`, `AM_HAL_CLKGEN_FREQ_HP192_HZ` (330P/510L only)
  and `AM_HAL_CLKGEN_FREQ_HP250_HZ`. Code uses those macros, including their
  FPGA-specific spellings, rather than duplicating silicon literals.
- AmbiqSuite `stable` at `b5375f2ab9054d774854fb5c223ef3129f261f42`,
  `mcu/apollo510/hal/am_hal_pwrctrl.c`, records that CPUHPFREQSEL was removed
  and 192 MHz is not supported on Apollo510 (CAYNSWS-1744/1465). HP uses
  HFRC2; mode selection does not restore its configured rate to 250 MHz.
  `modules/nsx-audio/src/apollo5/nsx_pdm_apollo5.c` can configure HFRC2 to
  196.608 MHz for audio. The same HAL's `am_hal_clkmgr_config_HFRC2()` stores
  pending configuration when there are no clock users; the peripheral's later
  clock request applies it. `am_hal_clkgen_private_hf2adj_apply()` and
  `am_hal_clkgen_private_hf2adj_disable()` control `CLKGEN.HF2ADJ0.HF2ADJEN`.
  `nsx_audio_init()` configures the clock before `nsx_audio_start()` enables
  PDM, so that deferred state is reachable through the NSX API.
- **Apollo510B SoC Datasheet**, DS-A510B-1p1p0, Table 9: LP CPU uses
  96 MHz HFRC and HP CPU uses 250 MHz HFRC2. Section 7.2.2 describes HFRC2
  adjustment for audio; it does not specify a 196.608 MHz HP CPU mode.
- **Apollo510 MCU Datasheet**, DS-A510-1p1p0, section 7.2.2 and Table 8,
  identifies HFRC2 as the HP CPU clock source.
- The same revision's `mcu/apollo330P_510L/hal/am_hal_pwrctrl.c` checks the
  configured PLLVCO rate before HP2, then requests it for the CPU.
  `am_hal_clkmgr_clock_config_get()` returns the stored configured frequency;
  the query is not a measurement of oscillator error.
- **Apollo330 Plus Series Datasheet**, DS-A330PS-0p9p0, revision 0.9.0,
  July 2026, power-mode and clocking sections: HP1 uses HFRC at 192 MHz;
  HP2 uses PLLVCO, nominally 250 MHz, with audio-dependent lower frequencies.
- **Apollo510 Lite Series Datasheet**, DS-A510LS-0p9p0, revision 0.9.0,
  July 2026, clocking table: CPU LP/HP1/HP2 sources are 96 MHz HFRC,
  192 MHz oscillator and 250 MHz PLLVCO respectively.

The current vendor CMSIS `SystemCoreClockUpdate()` implementation assigns
96 MHz regardless of the selected mode. `am_hal_clkgen_status_get()` likewise
reports the LP constant. Neither is a runtime-frequency query suitable for
implementing this fix. A fixed 250 MHz HP2 table is also insufficient when
audio configures a different PLLVCO frequency. Likewise, a fixed 250 MHz
Apollo510 HP value is wrong after HFRC2 is configured for audio.

This helper synchronizes the value at a successful mode-selection call.
Peripheral initialization that later reconfigures a shared CPU clock source
can make it stale again. Serialize clock configuration, reapply the desired
performance mode after such initialization, and then refresh timing settings;
there is no automatic notification from arbitrary HAL or audio calls.

## Validation

`tests/test_power_core_clock.py` executes the actual selector against a
controllable HAL. It checks mode selection, frequency publication order,
non-default HP2 PLL and active/pending HFRC2 audio rates, FPGA scaling, error propagation,
and rejected source configurations.
It separately configures the real board/module targets and compiles the
selector using generated include paths and definitions with GCC, ATfE and
ArmClang when installed. The GCC CI job requires its cross compiler.
Compilation and host mocks do not establish on-target tick accuracy.
