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

TempCo capture checks every trigger, cache operation and sample read. Each sample
has a 1,000-iteration trigger/poll budget with a nominal 1 us delay per iteration;
this bounds NSX polling, not waits inside vendor HAL calls. A failed operation,
missing sample or unexpected slot aborts the batch without applying trims. The
callback records the failure internally, reports it with
`NSX_POWERTEMPCO_DIAGNOSTICS`, and attempts the existing ADC sleep cleanup. A
later callback can retry; the first capture/handler error is preserved if sleep
cleanup also fails.

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

Startup ownership is shared with `nsx_system_init()`, `nsx_hw_init()` and
`nsx_minimal_hw_init()` on these families. Choose one entry point per reset,
then initialize drivers. A second valid request, or re-entry while startup is
running, returns `NSX_STATUS_STARTUP_REJECTED` without hardware effects. This
also applies after a reported setup failure; there is no public reset of the
guard. Core initialization does not clear it. Direct HAL/BSP calls bypass the
guard. Memory reductions are rejected as described below; constrained layouts remain
tracked in #137.

These adapters return the first failed configuration operation's status and
stop issuing subsequent operations. Performance and TempCo helper errors return
to the caller without entering `nsx_core_fail_loop()`. Applications must check
the returned status before continuing startup; applications that want fatal
handling must choose it explicitly.

Successful earlier writes remain in effect after a hardware failure. There is
no general rollback/retry guarantee: reset before retrying this startup API.
Input validation failures occur before ownership is claimed and can be corrected.
The
void BSP initializer cannot report its internal failures.
TempCo resource cleanup is described below.
Peripheral-field support is described below.

`tests/test_power_startup.py` calls the real public wrapper, family backend and
core lifecycle, with HAL calls controlled on the host. It checks invalid inputs,
startup without core initialization, supported configuration branches, and
failure at every observed status-returning step. The core's fatal endpoint is
replaced with an assertion so accidental fail loops fail the test. Eight
full-module board links and an additional AP4L real-header compile check cover
integration; no physical power sequencing is qualified by these host tests.

The `startup-ownership` hardware suite checks one real system initialization and
rejection of all four subsequent startup entry points on AP3 Blue Plus/AP4 Blue
KXR. Run `tests/hardware/test_ownership.py` with `--hardware`, `--probe SERIAL`
and `--qualification-profile BOARD-gcc-startup-ownership`, plus a fresh
`--qualification-report` path. It does not qualify peripheral or memory profiles.

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


## Apollo3/Apollo4 peripheral policy

Supported `need_*` flags skip optional NSX shutdowns when true. They do not
power up peripherals, initialize drivers, or undo the mandatory BSP baseline.
Configure once before drivers; BLE controller startup remains driver-owned,
including release of the reset asserted by Blue-board BSP initialization.

| Field | AP3/3P | AP4P | AP4L |
| --- | --- | --- | --- |
| `need_usb` | Unmanaged | USB and USBPHY | Unmanaged |
| `need_audadc` | Unmanaged | Audio ADC | Unmanaged |
| `need_iom` | Unmanaged | IOM0–7 | IOM0–7 |
| `need_uart` | Unmanaged | UART0/3 only | UART0/3 only |
| `need_crypto` | Unmanaged | Crypto shutdown | Crypto shutdown |
| `need_itm` | Unmanaged | DEBUG domain | DEBUG domain |
| `need_xtal` | Unmanaged | XTAL deep-sleep powerdown policy | Same as AP4P |
| `need_ble` | Unmanaged | Optional BLE GPIO shutdown | Optional BLE GPIO shutdown |
| `spotmgr_collapse` | Unmanaged | Unmanaged | Unmanaged |

Unmanaged fields are accepted for shared-preset compatibility. BSP effects still
apply independently of those fields. AP4 always selects LFRC for RTC and shuts
down UART1/2, IOS, ADC, MSPI, PDM and I2S blocks regardless of these flags.

`all_on` skips supported peripheral shutdowns except crypto; TempCo is false.
`audio` skips supported AUDADC shutdown, not PDM/I2S setup. `minimal` requests
supported shutdowns. Presets retain their existing values and are not guarantees
of peripheral readiness or minimum current. Check memory requirements separately.
Host tests exercise each flag independently and all three presets on four families.

The `usb-policy` hardware profile runs two fresh AP4 Blue KXR firmware images.
A test-only wrapper enables USB/USBPHY after real BSP initialization, then the
real NSX adapter preserves or shuts down those domains. Hardware status bits
verify both the initial state and the result; teardown shuts both domains down.
This checks power policy, not USB clocks, enumeration, or transfers.

```sh
uv run --group hardware --group ci pytest -q tests/hardware/test_usb_policy.py \
  --hardware --qualification-profile apollo4p_blue_kxr_evb-gcc-usb-policy \
  --qualification-report artifacts/usb-policy/report.json --probe YOUR_SERIAL \
  --gcc-root /usr/local/arm-gnu-toolchain
```

Reserve the board before running; this flashes it. Use a fresh report path.


## Apollo4 memory preflight

AP4P/L currently accepts only `small_tcm=false` and `need_ssram=true` through
`nsx_power_configure()`. Other combinations return `NSX_STATUS_INVALID_CONFIG`
before startup ownership, BSP initialization or any HAL operation. Correcting
the configuration allows a retry. This check also applies with
`NSX_DISABLE_API_VALIDATION`; custom linker scripts do not bypass it.

The default GCC allocator can use all 384 KiB DTCM; armclang places its stack
in upper DTCM. Neither default layout supports a 128 KiB reduction. Shared
sections and constructors are already live before this API runs, and empty
linked sections cannot prove that application-owned buffers are absent.
No reduced-memory layout/ownership contract is supported yet. This is a
conservative rejection, not automatic inspection of arbitrary application RAM.

Preset values remain unchanged. `all_on` satisfies the memory requirement.
On AP4, copy `minimal` or `audio` and retain shared SRAM explicitly:

```c
nsx_power_config_t cfg = nsx_power_audio;
cfg.need_ssram = true;
uint32_t status = nsx_power_configure(&cfg); // Check before starting drivers.
```

AP3/3P still leave both memory fields unmanaged. AP330/AP510-family policies
are unchanged. Accepted AP4 startup retains the mandatory vendor baseline and
existing DAXI setup; this does not qualify arbitrary pre-startup DMA, runtime
profile switching, custom allocators or sleep retention. Constrained layouts
and independent reductions remain a separate follow-up under #137.

The `memory-preflight` AP4 KXR hardware profile checks all three rejected
combinations with initialized shared data, shared BSS and a heap allocation
extending above 128 KiB DTCM. It verifies unchanged memory registers and
sentinels, zero BSP calls on rejection, then successful full-memory startup.
A test-only wrapper counts and forwards BSP calls; it does not replace the HAL.
Run after reserving the board, using a fresh report path:

```sh
uv run --group hardware --group ci pytest -q tests/hardware/test_memory_preflight.py \
  --hardware --qualification-profile apollo4p_blue_kxr_evb-gcc-memory-preflight \
  --qualification-report artifacts/memory-preflight/report.json --probe YOUR_SERIAL \
  --gcc-root /usr/local/arm-gnu-toolchain
```

This flashes the board. It tests rejection and corrected startup, not operation
with reduced RAM or physical AP4L behavior.
