# Clock/timebase qualification: first milestone contract

Work item: [#126](https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/126).
This first increment specifies the two-board measurement contract and adds
host-executed arithmetic intended for the target suite. It does not add a
hardware qualification profile or claim new silicon results.

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
| Same document, Table 11, p. 67 | HFRC sources list ±5%; 32.768 kHz reference depends on external crystal; PLLVCO entry describes a different typical rate | **Unresolved:** reconcile PLL source/divider terminology and frequency accuracy with Table 12 before accepting HP2 measured bounds |
| Same document, §11, pp. 79–80 | STIMER is a 32-bit selectable-source counter retained across warm resets | Same explicit initialization requirement; do not assume identical register layout across families |
| Shipped `boards/{apollo510b_evb,apollo330mP_evb}/bsp/am_bsp.h` under the SDK provider | Both declare XTAL low-speed mode and 32768 Hz | Candidate reference exists in board metadata; still verify fitted board, oscillator readiness and actual source configuration |
| Shipped `mcu/{apollo510,apollo330P}/hal/am_hal_stimer.h` | Separate `AM_HAL_STIMER_XTAL_32KHZ` and HFRC selections | Select/read back the crystal path; an HFRC timer is not independent of an HFRC CPU clock |

The locally available AmbiqSuite checkout at `b5375f2ab9054d774854fb5c223ef3129f261f42`
also contains a crystal-selected STIMER example in
`tests/widgets/apollo5/am_widget_stimer.c`. It is an implementation reference,
not proof that the shipped prebuilt HAL is identical. In particular,
`am_hal_stimer_config()` returns a configuration word, **not a HAL status code**;
check its documented semantics rather than asserting return value zero.

Before hardware acceptance implementation, resolve and record:

- Physical reference availability/readiness and its total accuracy on each EVB.
  A nominal BSP frequency is insufficient. Verify no fallback to another source.
- Actual DWT cycle-counter availability, divider/gating behavior and its relation
  to CPU cycles on these parts. Read back enables and verify advancement.
- HAL mode acknowledgement and settling for each transition, including ERR024
  disposition, with a deadline independent of the clock under test.
- AP330 HP2 PLL source, configuration/lock sequence and tolerance conflict above.
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
