# nsx-power

`nsx-power` provides NSX power-management helpers for supported AmbiqSuite Apollo targets.

The staged module currently targets these unified-repo boards:

- `apollo3_evb`
- `apollo3_evb_cygnus`
- `apollo3p_evb`
- `apollo3p_evb_cygnus`
- `apollo4l_evb`
- `apollo4l_blue_evb`
- `apollo4p_evb`
- `apollo4p_blue_kbr_evb`
- `apollo4p_blue_kxr_evb`
- `apollo510_evb`
- `apollo510b_evb`
- `apollo510dL_evb`
- `apollo330mP_evb`

The module keeps power configuration as a first-class capability:

- MCU performance mode selection
- shutdown of unused peripheral blocks
- SRAM and memory retention configuration
- deep-sleep helpers
- family-specific low-power controls where available
- GPIO-based energy monitor state signaling for external current probes
- staged power-profile register dumps for Apollo4 and Apollo5 families

Target-specific behavior is selected from `NSX_SOC_FAMILY` in CMake and from
the board/SoC compile definitions exported by `NSX_BOARD_FLAGS_TARGET`.

Apollo3 and Apollo4 support in the unified repo comes from the retired standalone `nsx-power` implementation, adapted onto the current `nsx_` API surface.

On Apollo4, `need_tempco` is implemented as a private `nsx-power` helper layered on top of `nsx::timer` rather than by reviving the legacy timer and harness globals. It is only supported on non-Lite Apollo4 parts whose trim revision reports TempCo support; unsupported parts fail the TempCo request instead of silently enabling partial behavior.

The staged helper surface also absorbs the old `nsx-utils` energy-monitor and
power-profile dump capabilities without reviving the rest of that mixed utility
bundle. `nsx-power` now exposes GPIO state signaling for external power tools,
plus target-specific power-profile dumps intended for low-power bring-up and
instrumentation.

The public API is exported as `nsx_power.h`.

## CPU clock after a performance change

On Apollo510/Apollo510B and Apollo330P/Apollo510L,
`nsx_power_set_performance_mode()` updates the CMSIS `SystemCoreClock` value
after the HAL successfully switches the CPU mode. On silicon, Apollo510 uses
the 96 MHz LP constant and queries configured HFRC2 for HP; HIGH and MAX
select the same HP mode. HFRC2 is normally 250 MHz, but audio can configure
it to 196.608 MHz. A stored audio setting takes effect only when adjustment
is active; pending configuration alone still reports nominal free-running
250 MHz. Apollo330P/510L use 96 MHz LP, 192 MHz HP1 (HIGH), and the
configured PLLVCO rate for HP2 (MAX). Audio can also lower that PLL rate.
Invalid or unavailable clock-source configuration fails before requesting the
mode change. FPGA builds use the HAL's scaled execution-rate constants.

The application owns SysTick and other timing peripherals. After a successful
switch, recalculate the SysTick reload using `SystemCoreClock`, and refresh
cached cycle-to-time conversions. Updating the CMSIS variable does not retune
an already-running RTOS tick. Direct HAL clock changes and the vendor
`SystemCoreClockUpdate()` routine do not go through this helper; the latter
still reports the startup frequency in the current SDK. Applications using
those lower-level paths must maintain their own clock synchronization. If
audio or peripheral initialization changes a shared CPU clock source, reapply
the desired performance mode afterward and then refresh timing settings.

The frequency table follows the per-part HAL constants and the Apollo510,
Apollo330 Plus and Apollo510 Lite datasheet clocking sections. In particular,
the 192 MHz HP1 mode belongs to Apollo330P/510L; it must not be inferred for
Apollo510 from a shared enum comment. See `docs/core-clock.md` for the source
revisions and validation scope.
