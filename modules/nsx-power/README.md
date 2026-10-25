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

## Compile/link regression coverage

`tests/test_power_link.py` builds the complete power module with its interrupt
and timer dependencies against shipped HAL/BSP headers and archives. The GCC
intake gate requires Apollo3, Apollo3 Plus, Apollo4 Plus, Apollo4 Blue Plus
KBR/KXR, Apollo330 Plus, Apollo510 and Apollo510B builds. Public monitor,
configuration, performance and profile entry points are retained in the ELF;
this check does not execute power changes or drive monitor pins.

Run locally with an Arm GNU toolchain installed:

```bash
NSX_REQUIRE_ARM_GCC=1 uv run --group ci pytest -q tests/test_power_link.py
```

The monitor uses Apollo3/3P's `g_AM_HAL_GPIO_OUTPUT` configuration and the
newer families' `am_hal_gpio_pincfg_output` configuration.

## Apollo3/Apollo4 selector contracts

`tests/test_power_selectors.py` executes the production selectors through the
public NSX header with a controllable HAL. Apollo3/3P use the shipped burst HAL
declarations; Apollo4 uses a small host HAL model. The complete eight-board
power link matrix checks the real family headers and archives separately.

LOW selects normal mode on AP3/3P and LP on AP4; HIGH/MAX select burst or HP.
Both families reject NOT_SET and unknown modes without touching the HAL and
leave `SystemCoreClock` unchanged. AP3/3P initializes burst support on its first
valid request, including LOW. Initialization errors propagate and remain
retryable; only successful, available initialization is cached. Successful
initialization remains cached if a later mode transition fails.

The required intake matrix covers translation, invalid inputs, status
propagation, retries and call ordering. It does not validate physical clock
rates, concurrent callers, sleep, or full platform power configuration.

## Apollo3/Apollo4 startup failure contract

`nsx_power_configure()` validates its configuration/API before dispatch. AP3/3P
and AP4 reject invalid performance modes before BSP initialization; TempCo is
rejected on AP3/3P and AP4L. The core lifecycle initialized flag is not a module
prerequisite, as specified by `nsx_core.h`.

These adapters return the first failed configuration operation's status and
stop issuing subsequent operations. Performance and TempCo helper errors return
to the caller without entering `nsx_core_fail_loop()`. Applications must check
the returned status before continuing startup; applications that want fatal
handling must choose it explicitly.

Successful earlier writes remain in effect after a hardware failure. There is
no general rollback/retry guarantee: reset or use board-specific recovery
before retrying. Input validation failures occur before hardware setup. The
void BSP initializer cannot report its internal failures.
[TempCo resource cleanup](https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/136) and
[configuration-field support](https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/137)
remain tracked follow-ups;
this contract is not a claim that every profile field is implemented.

`tests/test_power_startup.py` calls the real public wrapper, family backend and
core lifecycle, with HAL calls controlled on the host. It checks invalid inputs,
startup without core initialization, supported configuration branches, and
failure at every observed status-returning step. The core's fatal endpoint is
replaced with an assertion so accidental fail loops fail the test. Eight
full-module board links and an additional AP4L real-header compile check cover
integration; no physical power sequencing is qualified by these host tests.

## Apollo4 TempCo resource recovery

The AP4 TempCo helper owns ADC0 and timer 4 exclusively. It checks ADC setup and
arms the timer last. A failed setup returns its original error and releases
resources in timer-before-ADC order. A later enable finishes pending cleanup
before allocating again. Callback access is suppressed during setup/cleanup. A foreign timer owner
causes ADC resources to be released without touching that timer. Callback
failures remain observable even when subsequent ADC sleep succeeds. Failed ADC
enable attempts sleep cleanup. ADC sleep and its saved-state update preserve the
caller’s interrupt mask so a timer callback cannot observe a partial transition.

If cleanup fails, resources remain reserved until cleanup succeeds. If ADC
handle deinitialization fails, reset is required: the HAL can invalidate the
handle despite returning an error. This helper recovery does not roll back
other operations performed by `nsx_power_configure()`.

The private disable helper releases sampling resources; it does not restore
previous HAL trim settings or coordinate external users of the HAL TempCo
handler. Lifecycle calls are foreground-only. Host fault tests cover actual
TempCo and AP4 timer code. The AP4 KXR lifecycle smoke verifies setup, release
and reallocation; it does not qualify sampling accuracy, timer period, trim
adjustment, or sleep/wake behavior.

After reserving an AP4 Blue Plus KXR board, run the lifecycle smoke locally:

```sh
uv run --group hardware --group ci pytest -q tests/hardware/test_tempco.py \
  --hardware --qualification-profile apollo4p_blue_kxr_evb-gcc-tempco-lifecycle \
  --qualification-report artifacts/tempco/report.json --probe YOUR_SERIAL \
  --gcc-root /usr/local/arm-gnu-toolchain
```

This flashes the board. Use a fresh report path; the test requires working
factory TempCo calibration and fails if setup is unsupported. Interrupts are
masked during the short setup/release checks, so no temperature callback is
qualified. Period conversion (#139) and bounded capture/error handling (#140)
remain separate follow-ups.
