# CPU frequency synchronization in nsx-power

`nsx_power_set_performance_mode()` shares its Apollo510/Apollo330P/510L
implementation in `modules/nsx-power/src/nsx_power_performance.c`. It updates
the existing CMSIS `SystemCoreClock` variable after a successful HAL switch;
it does not own SysTick, RTOS scheduling, oscillator calibration or concurrent
HAL reconfiguration. Apollo3, Apollo4 and Atomiq110 retain their existing
platform implementations.

| Part | LOW | HIGH | MAX |
| --- | --- | --- | --- |
| Apollo510 / Apollo510B | HAL LP constant (nominal 96 MHz) | HAL HP constant (nominal 250 MHz) | Same as HIGH |
| Apollo330P / Apollo510L | HAL LP constant (nominal 96 MHz) | HAL HP1 constant (nominal 192 MHz) | Configured PLLVCO rate, 192–250 MHz on silicon |

The HP2 clock query runs before the mode switch. A failed query or a rate
outside the part's HP1/HP2 bounds returns an error before changing the CPU
mode. A failed HAL switch propagates its status and does not publish a new
frequency. As with the HAL itself, a failed hardware transition is not a
guarantee that the hardware remains in a known state; callers must handle the
error rather than continuing on an assumed frequency.

## Evidence and conflicting sources

- The vendored per-part `hal/mcu/am_hal_clkgen.h` headers define
  `AM_HAL_CLKGEN_FREQ_MAX_HZ`, `AM_HAL_CLKGEN_FREQ_HP192_HZ` (330P/510L only)
  and `AM_HAL_CLKGEN_FREQ_HP250_HZ`. Code uses those macros, including their
  FPGA-specific spellings, rather than duplicating silicon literals.
- AmbiqSuite `stable` at `b5375f2ab9054d774854fb5c223ef3129f261f42`,
  `mcu/apollo510/hal/am_hal_pwrctrl.c`, records that CPUHPFREQSEL was removed
  and 192 MHz is not supported on Apollo510 (CAYNSWS-1744/1465).
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
audio configures a different PLLVCO frequency.

## Validation

`tests/test_power_core_clock.py` executes the actual selector against a
controllable HAL. It checks mode selection, frequency publication order,
non-default HP2 PLL rates, error propagation, and rejected PLL configurations.
It separately configures the real board/module targets and compiles the
selector using generated include paths and definitions with GCC, ATfE and
ArmClang when installed. The GCC CI job requires its cross compiler.
Compilation and host mocks do not establish on-target tick accuracy.
