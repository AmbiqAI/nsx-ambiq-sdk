# Clock/timebase test contract

Work item: [#126](https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/126).
This contract defines the experimental two-board clock and application-reloaded
SysTick checks. It does not establish calibrated hardware qualification.

Milestone 1 covers `apollo510b_evb` and `apollo330mP_evb`, nominal CPU modes and
application-reloaded SysTick. AP3/AP4/AP510 expansion is milestone 2. Delay
helpers, RTOS scheduling, sleep/cold boot, audio clock adjustment and generated
neuralspotx applications remain separate work.

## Source evidence

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

## Reference design

The AP510B Rev2.0 and AP330 Plus Rev1.0–3.0 EVB schematics (sheet 4)
specify Y2, TXC 9HT12-32.768KDZY-T, with two 10 pF load capacitors.
[TXC 9HT12 revision 1.05](https://txccrystal.com/images/pdf/9ht12.pdf)
specifies ±20 ppm initial tolerance at 25°C and 7 pF load for this part.
Assembly substitutions, load pulling, temperature and aging are not bounded
by schematic identity alone; ±50 ppm below remains a fixture assumption.

AP330 HP2 uses PLL VCO, not a divided peripheral output. Configure 250 MHz
with HFRC192DIV4 explicitly and verify lock and the divider ratio:
`Fvco = 48 MHz * (FBDivInt + FBDivFrac / 2^24) / RefDiv`.
Do not infer crystal accuracy for this HFRC-derived clock or rely on automatic
reference selection. Table 12 and shipped CLKMGR bounds support this setting;
Table 11's typical PLL rate is not the CPU maximum.

## Acceptance scope and lab conditions

This milestone tests SDK integration on awake EVBs in an ordinary indoor lab,
with an ambient target of 20–30°C, normal board supply, no audio adjustment and
no external workload. Record board/probe identity, firmware/SDK revisions,
reset method, debugger attachment and whether lab temperature was observed or
merely assumed. Do not describe these runs as temperature/voltage qualification,
cold-start qualification, precision frequency calibration or full ERR024 coverage.

There are three distinct conclusions:

| Check | Acceptance | What a pass establishes |
| --- | --- | --- |
| SDK mode and publication | Successful API return; actual mode/readiness/source matches the requested supported mode; exact nominal `SystemCoreClock` | Correct SDK selection and reporting on this run |
| Application SysTick | Expected reload and bounded interrupt accounting across each transition | The application can keep its tick correct when changing CPU speed |
| Physical-rate screen | Each valid capture is within ±5.2% of the selected nominal rate, under the reference assumptions below | A coarse clock regression screen, conditional on the stated measurement model |

The reference budget for the screen is **±50 ppm total assembled-reference
error**, including load, temperature and aging. This is a declared fixture
assumption, **not a proven property of these boards**. The source tolerance,
crystal part, mode/source readbacks and capture checks are evidence; they do not
independently prove that assumption. A report without independent reference
validation must say **conditional/experimental**, record the assumption, and
must not claim calibrated frequency accuracy or unconditional clock qualification.
This does not prevent completing the SDK functional checks or reviewing this PR.
If absolute-frequency qualification is required later, supply a traceable
reference measurement/error budget covering the actual board and conditions;
self-comparison of DWT with the same crystal is not that evidence.

### Frequency budget and limits

Use the documented ±5% source allowance for HFRC/HFRC2 and the HFRC-derived,
locked AP330 PLL path. The latter also requires exact source/divider validation:
a broad frequency screen alone cannot distinguish 245.76 MHz from 250 MHz.
For a 4096-tick window, budget two reference ticks of combined endpoint
uncertainty and up to 2048 DWT cycles per endpoint read. Reject larger capture
apertures; do not expand the allowed error to accommodate a slow read.

Let `F` be nominal CPU Hz, `N` the measured reference interval, `R=32768`,
`e_cpu=±0.05`, `e_ref=±0.000050`, `d_ref=±2` ticks and `d_cpu=±4096` cycles.
The worst-case estimated rate under this model is:

```text
estimated_hz = F * (1 + e_cpu) * (N + d_ref) / (N * (1 + e_ref))
               + d_cpu * R / N
```

At the shortest accepted window (`N=4096`), evaluating all endpoint/error signs
with exact rational arithmetic gives these outward-rounded deviations from nominal:

| Nominal | Lowest estimate | Highest estimate | Screen limits (Hz) |
| --- | --- | --- | --- |
| 96 MHz | −5.08527% | +5.09066% | 91,008,000–100,992,000 |
| 192 MHz | −5.06821% | +5.07359% | 182,016,000–201,984,000 |
| 250 MHz | −5.06425% | +5.06963% | 237,000,000–263,000,000 |

Thus ±5.2% covers the combined stated budget without using the measured EVB
results to choose the limit. `tests/test_clock_contract.py` checks every sign
combination, counter-wrap bounds and separation of adjacent nominal modes.
The bound applies to an averaged frequency, not instantaneous PLL jitter.
Violating the model's assumptions invalidates that interpretation of the screen;
a passing arithmetic test does not establish real-world reference accuracy.

### Startup, transition and capture rules

| Stage | Required behavior | Bound / failure behavior |
| --- | --- | --- |
| Reference setup | Explicitly enable XTAL mode; clear/unfreeze STIMER; read back the XTAL-32K source and verify DWT enable/advancement | Any setup/readback failure invalidates the run; no implicit HFRC fallback |
| Warm pre-roll | Discard the first 16,384 advancing reference ticks before taking samples | About 0.5 s nominal; at most 2,000,000 polls. This is a test policy, not a manufacturer startup guarantee |
| Mode switch | Check API status, MCUPERFACK and actual MCUPERFSTATUS; AP510B HP also needs HFRC2READY and inactive audio adjustment; AP330 HP2 needs PLL lock and exact configured source/ratio | Additional readiness observation is bounded by 327 reference ticks (about 10 ms) and 2,000,000 polls. A stuck HAL call is covered by the host deadline |
| Acquisition | Three separate windows per row; read back the reference source; bracket reference reads with DWT; use no interrupt other than the tested SysTick | Each window 4096–4104 reference ticks; endpoint aperture ≤2048 cycles; stop at 300,000,000 elapsed DWT cycles or 2,000,000 polls |
| Counter validity | Check the cycle deadline on every acquisition poll; do not reset/reconfigure counters mid-window, sleep, single-step or halt the target | Reject stalls, invalid windows, unexpected source state or evidence of interruption; never reuse a previous sample |
| Host supervision | No target halts while measuring; poll only completion state until the target publishes its result | 40 s firmware-completion deadline; existing parent worker cap adds 60 s for transport/setup. Neither is a measurement reference or a sub-window wrap proof |
| Cleanup | Disable/clear SysTick; restore interrupt masks and LOW mode; verify return status, nominal publication and actual LOW status | Cleanup failure invalidates the run even when prior assertions passed |

The cycle-wrap proof additionally assumes bounded execution between polls: no
uncontrolled ISR, breakpoint or stalled bus operation hidden between snapshots.
The coarse host deadline alone cannot prove this. If those conditions cannot be
maintained, discard the capture and use a separately bounded acquisition method.
The half-second pre-roll proves only reference progress; it does not certify
crystal cold-start settling. An unstable or interrupted run fails the regression
check rather than triggering a longer, unreviewed delay or wider limit.

SWD completion polling is permitted for these debugger-attached functional runs.
Read-aperture checks bound its effect at the endpoints; DWT counts CPU cycles
including execution stalls between them. Do not use these results as throughput,
sleep or debugger-free timing measurements. A claim about those environments
requires a separate test design.

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
mode use at least three independently captured windows. The readiness and observation bounds above apply to every row; no additional
unrecorded settling delay is implied.
HP at 196.608 MHz on AP510B is not a test case. Pending/active/released audio
adjustment interactions remain identified gaps in #72; this milestone cannot
close all of #72 by itself.

## Measurement and arithmetic contract

Maintain two independent verdicts:

1. API publication: `SystemCoreClock` equals the nominal value for the selected
   supported source/mode. This check does not measure oscillator accuracy.
2. Conditional physical-rate screen: measured CPU cycles over independently
   sourced reference ticks fall within the explicit budget above. Record the
   reference assumption and endpoint uncertainty; do not derive expected elapsed
   time from `SystemCoreClock` or host USB timing.

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

The firmware must verify application-reloaded SysTick. For each
window compare the interrupt count times the expected CPU cycles per tick
(`nominal_hz / 1000`) with measured DWT cycles, allowing two boundary interrupts
plus 4096 endpoint cycles. This checks reload/interrupt accounting; SysTick and
DWT share a CPU clock and are not independent frequency standards. The separate
crystal-referenced CPU screen supplies the conditional physical-rate evidence. It must demonstrate three artifact-only negative controls: stale/wrong
publication, omitted reload across a frequency-changing transition and wrong
measurement scaling. A normal restoration run follows controls.

## Fixed harness versus candidate SDK

The comparison entry point must take two explicit roots: an immutable
test-harness checkout and the selected candidate SDK checkout. Resolve firmware,
Unity, reporter, acceptance definitions and test preparation from the harness;
resolve production board/CMake/provider/module inputs from the selected SDK.
Record both revisions/dirty states and hashes separately. No implicit sibling
resolution and no silent import of candidate tests. Keep the existing default
of testing this checkout compatible for normal standalone use.

The current builder uses one root for both purposes. This separation needs an
explicit API change plus build/provenance regression tests before baseline versus
candidate claims; it is not implemented by this contract PR.

## Completion criteria

Require AP510B and AP330 results for every transition row, tick checks,
negative controls, cleanup and fixed-harness comparison. Preserve existing
startup/core profiles and the conditional reference qualification until
independent evidence supports promotion. Family expansion is a separate step.
Store dated lab observations, captures and review records with run artifacts
and PR evidence rather than this contract.
