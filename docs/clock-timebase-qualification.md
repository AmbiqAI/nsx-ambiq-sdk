# Clock/timebase qualification: first milestone contract

Work item: [#126](https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/126).
This first increment specifies the two-board measurement contract and adds
host-executed arithmetic intended for the target suite. Bounded, artifact-only
board diagnostics now establish reference feasibility below. They do not add a
hardware qualification profile or complete the transition matrix.

Milestone 1 covers `apollo510b_evb` and `apollo330mP_evb`, nominal CPU modes and
application-reloaded SysTick. AP3/AP4/AP510 expansion is milestone 2. Delay
helpers, RTOS scheduling, sleep/cold boot, audio clock adjustment and generated
neuralspotx applications remain separate work.

## Source evidence and unresolved prerequisites

The inspected SDK baseline is `cab4033dfffa809e95eb075df73b29252bd23809`.
The source documents below are identified by revision and section rather than
copied into this repository. They establish candidate clocks, not physical EVB
readiness or a complete measurement error budget.

| Source | Evidence | Implication |
| --- | --- | --- |
| Apollo510B SoC Datasheet, DS-A510B-1p1p0, Tables 8/9, pp. 65–66 | Nominal LP 96 MHz and HP 250 MHz; HFRC/HFRC2 source tolerance listed as ±5%; external 32.768 kHz crystal requirement depends on the crystal | Keep nominal API assertions separate from measured-frequency bounds; do not promote ±5% alone into the entire test tolerance |
| Same document, §4.3.1 and ERR024 note, pp. 47–48 | HP request acknowledgement and HFRC2 readiness/settling matter | A successful API return or request bit alone is insufficient proof that measurement may start; review packaged HAL behavior and applicable errata |
| Same document, §10, pp. 77–78 | STIMER is a 32-bit selectable-source counter retained across warm resets | Explicitly initialize/read back its source and state; do not rely on debugger reset to clear it |
| Apollo330 Plus Series Datasheet, DS-A330PS-0p9p0, Table 12, p. 68 | LP/HP1/HP2 CPU clocks nominally 96/192/250 MHz; HP2 requires PLL configured/enabled before transition | HP2 needs a separately verified PLL setup and lock/readiness check |
| Same document, Table 11, p. 67 | HFRC sources list ±5%; 32.768 kHz reference depends on external crystal; PLLVCO entry describes a different typical rate | The typical PLL rate is not the CPU maximum: Table 12 and the shipped CLKMGR 60–250 MHz range support 250 MHz VCO; physical accuracy depends on the selected PLL reference (see below) |
| Same document, §11, pp. 79–80 | STIMER is a 32-bit selectable-source counter retained across warm resets | Same explicit initialization requirement; do not assume identical register layout across families |
| Shipped `boards/{apollo510b_evb,apollo330mP_evb}/bsp/am_bsp.h` under the SDK provider | Both declare XTAL low-speed mode and 32768 Hz | Candidate reference exists in board metadata; still verify fitted board, oscillator readiness and actual source configuration |
| Shipped `mcu/{apollo510,apollo330P}/hal/am_hal_stimer.h` | Separate `AM_HAL_STIMER_XTAL_32KHZ` and HFRC selections | Select/read back the crystal path; an HFRC timer is not independent of an HFRC CPU clock |

The locally available AmbiqSuite checkout at `b5375f2ab9054d774854fb5c223ef3129f261f42`
also contains a crystal-selected STIMER example in
`tests/widgets/apollo5/am_widget_stimer.c`. It is an implementation reference,
not proof that the shipped prebuilt HAL is identical. In particular,
`am_hal_stimer_config()` returns a configuration word, **not a HAL status code**;
check its documented semantics rather than asserting return value zero.

## Reference feasibility checked on 2026-10-24

Artifact-only firmware linked the shipped HAL/BSP and `nsx::power` from PR127
head `e1c4278`. It used full BSP initialization, explicitly enabled EXTCLK32K in
crystal mode through MCUCTRL, cleared/unfroze STIMER and selected
`AM_HAL_STIMER_XTAL_32KHZ`. Both boards reported crystal mode and STCFG=3.
DWT CYCCNT was explicitly enabled and advanced. There were three 8192-reference-
tick windows per requested mode, a bounded acquisition loop, and a 30-second
host completion deadline. No debugger halt was requested during acquisition.
The existing runner polled the completion word over SWD.

These are observations scaled using **nominal** 32768 Hz, not calibrated clock
accuracy or new pass limits. The diagnostic asserted successful configuration,
reference/cycle advancement and mode API returns; it logged frequency data and
readiness registers without applying a frequency tolerance. LOW restoration
returned success on both boards. Shared lab claims were released afterward.

| Board / probe | Sequence | Observed MHz (three-window range) | Nominal publication |
| --- | --- | --- | --- |
| AP510B / 1160002954 | LOW | 95.677–95.683 | 96 MHz |
| Same | HIGH | 248.956–248.982 | 250 MHz |
| Same | MAX | 248.963–248.977 | 250 MHz |
| Same | LOW restoration | 95.689–95.695 | 96 MHz |
| AP330 Plus / 1160003058 | LOW | 95.924–95.932 | 96 MHz |
| Same | HIGH | 191.856–191.868 | 192 MHz |
| Same | MAX | 249.812–249.841 | 250 MHz |
| Same | LOW restoration | 95.923–95.925 | 96 MHz |

### AP330 PLL resolution

The shipped `am_hal_syspll.h` distinguishes VCO from the divided peripheral
outputs: `Fvco = Fref * (FBDivInt + FBDivFrac / 2^24) / RefDiv`.
The CPU HP2 path uses VCO, not POSTDIV. The internal
[Camaro clock specification](https://ambiqmicro.atlassian.net/wiki/spaces/Camaro/pages/3738664961/Clocking),
“HP2 Mode and PLL Handling”, explicitly permits 250 MHz without audio and
explains the temporary 192 MHz execution path while PLL lock is pending.
Together with datasheet Table 12 and the shipped CLKMGR bounds, this resolves
whether 250 MHz is an intended HP2 setting. It does not correct Table 11 or
establish a PLL accuracy specification.

For the measured MAX row, configuration requested 250000000 Hz. The hardware
readback was PLLSTAT=1, PLLCTL0=0x2000020e, PLLDIV0=0 and PLLDIV1=0x007d0018:
FREFSEL=HFRC192DIV4, integer feedback=125, reference divider=24. Thus nominal
48 MHz * 125 / 24 = 250 MHz. **This run used HFRC as the PLL reference, not the
RF crystal.** Any measured bound must include HFRC source accuracy and PLL/error-
measurement contributions; crystal-level accuracy cannot be assumed for HP2.
Do not rely on auto-selection staying the same in a future SDK: establish and
assert the allowed reference in the eventual qualification fixture.

### Readiness and timeout evidence

Disassembly of the shipped AP510 HAL confirms that the HP path forces HFRC2
when needed, delays one microsecond, waits for HFRC2READY with a 15-microsecond
HAL timeout, and checks readiness before changing performance mode. It also
polls MCUPERFACK and checks MCUPERFSTATUS. This is implementation evidence for
the ERR024 sequence, not exhaustive erratum validation. The recorded HP rows
had HFRC2READY set and performance acknowledgement/status matching HP.

The shipped AP330 `am_hal_syspll_lock_wait` calculates its timeout as
`ceil(lock_cycles * RefDiv / 12)` microseconds, where lock_cycles is 1000 or
1875 according to VCO selection. Omitting RefDiv would give the wrong timeout.
The observed HP2 configuration uses RefDiv=24 and the high VCO selection,
therefore this HAL timeout is 3750 microseconds. PLLSTAT.LOCK and actual
MCUPERFSTATUS must be checked before an acceptance window, even if the mode API
returns success. HAL delay-based timeout values are not an independent timing
oracle; retain a separately bounded target acquisition and host process deadline.

Evidence is retained outside the repository under
`artifacts/nsx/sdk-clock-contract-2026-10-24/` in the enclosing NSX workspace:
`reference-probe/` contains diagnostic sources; `ap510b-run2/` and `ap330-run1/`
contain build manifests, ELF/source/library hashes, raw Unity logs and execution
metadata. `source-evidence/` contains the inspected archive disassembly. These
were debugger-attached reset runs, not cold boots, PVT testing, or the complete
acceptance matrix. The diagnostic does not yet use the arithmetic helper.

## Remaining acceptance prerequisites

Before hardware acceptance implementation, resolve and record:

- The fitted reference crystal’s total accuracy on each EVB (component, load,
  temperature/aging assumptions), and startup stabilization policy. Crystal-mode
  readback and advancing samples establish feasibility, not calibrated accuracy.
  The available quick-start guides do not supply this complete error budget.
- Preserve explicit DWT enable/advancement and clock-source readback checks in
  the executable fixture; the feasibility results cover running, awake CPUs only.
- HAL mode acknowledgement and settling for each transition, including ERR024
  disposition, with a deadline independent of the clock under test.
- Specify the AP330 PLL reference explicitly and derive the acceptance bound
  from that reference; verify lock and divider readback for every HP2 entry.
- Reference capture aperture, polling/interrupt overhead and debugger/SRAM-read
  effects. No debugger halts during windows; completion polling policy must be
  justified or adapted before claiming timing accuracy.

These are explicit gates for the next PR, not unspecified tolerances to be
filled with values that happen to make a board pass.

## Required nominal transition matrix

Expected success means `NSX_STATUS_SUCCESS`. Nominal publication is exact;
physical frequency acceptance uses separately reviewed bounds. Each row must
record actual starting mode and clock-source state, not just assume a preceding
row completed. Setup and cleanup failures invalidate the run.

| Board | Established start | Public API request | Expected end / nominal publication | Restoration |
| --- | --- | --- | --- | --- |
| AP510B | LOW, audio adjustment inactive | LOW | LOW / 96 MHz (idempotent request) | LOW |
| AP510B | LOW | HIGH | HP / 250 MHz | LOW |
| AP510B | HP | HIGH | HP / 250 MHz (idempotent request) | LOW |
| AP510B | HP | MAX | HP / 250 MHz (alias) | LOW |
| AP510B | LOW | MAX | HP / 250 MHz (alias entry) | LOW |
| AP510B | HP | LOW | LOW / 96 MHz | LOW |
| AP330 Plus | LOW, explicit nominal 250 MHz PLL configuration established for MAX rows | LOW | LOW / 96 MHz | LOW |
| AP330 Plus | LOW | HIGH | HP1 / 192 MHz | LOW |
| AP330 Plus | HP1 | HIGH | HP1 / 192 MHz (idempotent request) | LOW |
| AP330 Plus | LOW or HP1 (separate rows in executable matrix) | MAX | HP2 / configured 250 MHz | LOW |
| AP330 Plus | HP2 | MAX | HP2 / configured 250 MHz (idempotent request) | LOW |
| AP330 Plus | HP2 | HIGH | HP1 / 192 MHz | LOW |
| AP330 Plus | HP1 or HP2 (separate rows in executable matrix) | LOW | LOW / 96 MHz | LOW |
| Both | Each already qualified mode | NOT_SET | Current shared adapter normalizes to LOW / 96 MHz; preserve this existing tested behavior unless deliberately revised | LOW |

The shared adapter also falls back to LOW for unknown enum values. The public
contract for invalid enums needs an explicit decision before introducing a
hardware assertion; do not assume they are rejected or silently change production
behavior in a test PR. Existing host tests cover rejected clock-source settings
and HAL failures. Only add an on-target rejection row once its setup and exact
return code are supported; no unsafe source setting is needed just to trigger an
error.
Repeat the full nominal sequence after returning to LOW. For every measured
mode use at least three independently captured windows. Row-specific readiness
and maximum settling bounds remain prerequisites above, not implicit delays.
HP at 196.608 MHz on AP510B is not a test case. Pending/active/released audio
adjustment interactions remain identified gaps in #72; this milestone cannot
close all of #72 by itself.

## Measurement and arithmetic contract

Maintain two independent verdicts:

1. API publication: `SystemCoreClock` equals the nominal value for the selected
   supported source/mode. This check does not measure oscillator accuracy.
2. Physical rate: measured CPU cycles over independently sourced reference ticks
   fall within reviewed bounds. Account for oscillator/reference accuracy and
   endpoint uncertainty separately; do not derive expected elapsed time from
   `SystemCoreClock` or host USB timing.

`tests/hardware/clock/measurement.h` implements only bounded counter arithmetic:
unsigned differences allow a counter to cross zero; an independently specified
maximum window, maximum CPU rate and minimum physical reference rate must rule
out a complete CPU-counter revolution. Stalls, overlong windows and impossible
rates reject the sample. Cross-products use 64-bit arithmetic and retain
fractional-Hz information at acceptance boundaries. The helper neither selects
clocks nor supplies silicon tolerances.

The caller must independently bound the actual acquisition duration: two
snapshots cannot distinguish multiple counter wraps or prove that a reference
never stopped. Reference capture aperture must be accounted for in those
bounds. The host subprocess deadline remains necessary if the target/reference
stalls. Rejected samples must not be consumed as valid prior results.

`tests/test_clock_measurement.py` executes this target-usable C code with the host
compiler and UBSan. It covers a simultaneous reference/DWT wrap, nominal 96/250
MHz examples, stalled counters, ambiguous/excessive windows, wrong reference
scaling, stale nominal expectations, fractional boundaries and full-width
products. Its example bounds are synthetic arithmetic inputs, not EVB policy.
The same test module also cross-compiles the helper for Cortex-M55 with GCC.
Run locally with `uv run --group ci pytest -q tests/test_clock_measurement.py`.

The firmware PR must additionally verify application-reloaded SysTick against
the same independent reference, including interrupt-count and endpoint-error
bounds. It must demonstrate three artifact-only negative controls: stale/wrong
publication, omitted reload across a frequency-changing transition and wrong
measurement scaling. A normal restoration run follows controls.

## Fixed harness versus candidate SDK

The future comparison entry point must take two explicit roots: an immutable
test-harness checkout and the selected candidate SDK checkout. Resolve firmware,
Unity, reporter, acceptance definitions and test preparation from the harness;
resolve production board/CMake/provider/module inputs from the selected SDK.
Record both revisions/dirty states and hashes separately. No implicit sibling
resolution and no silent import of candidate tests. Keep the existing default
of testing this checkout compatible for normal standalone use.

The current builder uses one root for both purposes. This separation needs an
explicit API change plus build/provenance regression tests before baseline versus
candidate claims; it is not implemented by this contract PR.

## Completion and review

This first PR is complete when the issue scope, traceable matrix, unresolved
reference gates and arithmetic checks are reviewable and local checks pass.
It is **not** milestone-1 hardware completion. The next PR resolves reference
feasibility, readiness/tolerances and the executable per-row matrix before adding
an additive qualification profile. Preserve existing startup/core profiles.

Milestone 1 requires AP510B and AP330 results for every required row, tick checks,
negative controls, cleanup and fixed-harness comparison evidence. Family
expansion remains milestone 2; unavailable expansion boards do not block the
first milestone. Independent review and user approval precede merges.
