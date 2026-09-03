/**
 * @file nsx_npu_timebase.c
 * @author Ambiq
 * @brief Atomiq110 timebase hooks for the Ethos-U85 wait semaphore.
 *
 * nsx-ethos-u-driver honours the `timeout` argument of
 * `ethosu_semaphore_take()` only when the integrator supplies a time source
 * through two weak hooks (see `nsx_ethos_u.h`, "Optional timebase hooks").
 * Without them a wedged NPU -- one that never raises its interrupt -- hangs the
 * caller forever instead of surfacing `ETHOSU_JOB_RESULT_TIMEOUT` and taking
 * the `ethosu_soft_reset()` recovery path. This file provides the strong
 * overrides for Atomiq110.
 *
 * ## Design
 *
 * The time source is STIMER, a free-running 32-bit counter whose clock tap
 * (STCFG.CLKSEL) is shared with the application. This module publishes a
 * **fixed-rate virtual clock** rather than the raw counter:
 *
 *  - `nsx_ethos_u_ticks_per_ms()` always reports 1000 once armed, i.e. the
 *    virtual tick is 1 us.
 *  - `nsx_ethos_u_ticks()` accumulates the raw counter's advance since the
 *    previous call, converted at the rate of the tap STIMER is on, into a
 *    64-bit microsecond total. An interval across which the tap changed, or one
 *    spent on a tap whose rate cannot be derived (NOCLK, the CTIMER feeds), is
 *    not converted: it is dropped and the reference re-anchored. The virtual
 *    clock can therefore lag real time by up to one poll interval per tap
 *    change, but it never runs ahead of it.
 *
 * Converting per call, from the live CLKSEL, is what makes the deadline immune
 * to the application reconfiguring STIMER: whether it moves the tap before
 * `nsx_npu_init()`, between inferences, or in the middle of one, every interval
 * that is converted is converted at the one rate that governed it, and the span
 * the driver computed from `ticks_per_ms()` stays valid. A cached rate cannot
 * give that guarantee -- a tap change from 488 kHz to 7.8 MHz against a cached
 * 488 kHz over-reports elapsed time 16x and aborts a healthy inference.
 *
 * The other two hazards a shared counter presents are handled in the delta
 * rule (see `nsx_npu_tb_raw_elapsed()`):
 *
 *  - A 32-bit wrap and `am_hal_stimer_counter_clear()` both show up as the raw
 *    value going backwards. The rule credits the *smaller* of the two
 *    interpretations, so a clear can never be mistaken for a ~2^32-tick wrap.
 *    Whichever it really was, the error is an under-report -- a late timeout,
 *    never a spurious one.
 *  - The counter read itself is kept *outside* the critical section (it is a
 *    triple read of a slow always-on register, and this hook is polled in a
 *    tight loop for the whole inference). A read that is overtaken by another
 *    caller's update is recognised as slightly stale and ignored.
 *
 * Contract points from `nsx_ethos_u.h` honoured here:
 *
 *  - `ticks()` wraps only at 2^64 (a 64-bit microsecond count).
 *  - `ticks()` never returns 0 while armed: the result carries a `| 1` bias,
 *    which the contract explicitly sanctions.
 *  - Both hooks or neither: until armed, both report 0 and every wait degrades
 *    to upstream's unbounded behaviour. `nsx_npu_init()` logs when that
 *    happens, and `nsx_npu_timebase_status()` exposes why.
 *
 * TODO(atomiq110 silicon bring-up): CLKMGR's HFRC query on the
 * atomiq110_fpga_turbo image reliably reports status=SUCCESS/hz=0 (the clock
 * exists but is not calibrated). Falling back directly to the HAL's
 * silicon-nominal HFRC estimate overestimates the real tap rate on this
 * image by ~10x -- correct in direction (elapsed time under-reported, timeout
 * fires late rather than early, see above) but needlessly imprecise.
 * `nsx_npu_tb_fpga_calibrate_hfrc_root_hz()` replaces that guess, once at
 * init, with a runtime measurement of the tap against the CPU cycle counter,
 * gated entirely behind the upstream `ATOMIQ11X_FPGA` macro (see that
 * function's own comment). It is FPGA-only bring-up plumbing: once real
 * atomiq110 silicon is available and CLKMGR's HFRC report is verified
 * trustworthy there, re-check whether this self-calibration path is still
 * needed and remove it (and this note) if not -- `ATOMIQ11X_FPGA` will not be
 * defined on a silicon build, so the code already goes dead on its own, but
 * the dead branch and its test coverage should not linger indefinitely.
 *
 * @copyright Copyright (c) 2026, Ambiq Micro, Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include "am_mcu_apollo.h"

#include "nsx_ethos_u.h"
#include "nsx_npu.h"
#include "nsx_npu_timebase.h"

//*****************************************************************************
//
// Tick source selection
//
//*****************************************************************************

//
// Default STIMER source when nothing else has claimed the block: the HFRC
// divider tap the register map calls HFRC_488_KHZ. (AM_HAL_STIMER_HFRC_375KHZ
// is a legacy Apollo3-era alias for that CLKSEL value; the tap really is
// ~488 kHz on this part.)
//
// HFRC was chosen over the 32 kHz crystal deliberately. Both candidates trade
// one uncertainty for another on the FPGA:
//
//   - XTAL_32KHZ has a rate that is certain by construction (a crystal is a
//     crystal on FPGA and on silicon) but liveness that is not: nothing
//     guarantees the LS crystal is populated and running on
//     atomiq110_fpga_turbo, and a stopped counter is the one failure this
//     module exists to prevent.
//   - The HFRC taps have liveness that is certain -- HFRC clocks the core, so
//     if code is executing, HFRC is running -- and a rate that is only as
//     accurate as what CLKMGR reports.
//
// For a hang detector, "certainly alive, approximately calibrated" beats
// "exactly calibrated, possibly stopped": a mis-scaled deadline still fires,
// just early or late, whereas a stopped counter never fires at all.
//
#define NSX_NPU_TB_STIMER_CFG (AM_HAL_STIMER_HFRC_375KHZ | AM_HAL_STIMER_CFG_RUN)

//
// Rate of the virtual clock published to the driver, in ticks per millisecond.
// 1000 makes one virtual tick one microsecond, which is finer than any STIMER
// tap on this part, so no source rate is degraded by the conversion.
//
#define NSX_NPU_TB_VIRTUAL_TICKS_PER_MS (1000U)
#define NSX_NPU_TB_VIRTUAL_HZ           (1000U * NSX_NPU_TB_VIRTUAL_TICKS_PER_MS)

//
// Bounds on the liveness-probe busy wait, in microseconds. The probe waits for
// roughly two ticks of the detected rate; the floor keeps fast taps from
// probing a sub-tick interval, the ceiling keeps a misdetected slow tap from
// stalling init.
//
#define NSX_NPU_TB_PROBE_US_MIN (100U)
#define NSX_NPU_TB_PROBE_US_MAX (8000U)

#ifdef ATOMIQ11X_FPGA
//
// Calibration window for nsx_npu_tb_fpga_calibrate_hfrc_root_hz(), in
// microseconds. Runs once per boot from nsx_npu_timebase_init() only (never
// from the per-tick nsx_npu_tb_source_hz() path), so a busy wait here is a
// one-time init cost, not a recurring one. 10ms gives a comfortably
// measurable STIMER delta even on the slowest HFRC-derived tap this module
// recognises (HFRC_7_8125MHZ / 64 ~= 122 kHz at the silicon-nominal root, and
// considerably slower than that on this FPGA image).
//
#define NSX_NPU_TB_FPGA_CAL_WINDOW_US (10000U)
#endif

//
// A raw read that lands *behind* the last recorded value by no more than this
// many milliseconds' worth of ticks is treated as a stale concurrent read (see
// nsx_npu_tb_raw_elapsed()) rather than as a counter clear. It bounds the one
// cost of the stale-read guard: a genuine clear performed while the counter
// was still this close to zero is credited late by at most this long.
//
#define NSX_NPU_TB_STALE_WINDOW_MS (10U)

//*****************************************************************************
//
// Module state
//
//*****************************************************************************

//
// Published state. NSX_NPU_TIMEBASE_ARMED is the only value under which the
// hooks report a live clock; every other value makes both hooks return 0, the
// driver's "no time source" sentinel.
//
static volatile nsx_npu_timebase_status_e g_nsx_npu_tb_status = NSX_NPU_TIMEBASE_NOT_INITIALIZED;

//
// Virtual-clock accumulator. All four fields are read-modify-written together
// inside the critical section in nsx_ethos_u_ticks(); nothing else touches
// them once armed.
//
static uint32_t g_nsx_npu_tb_last_raw = 0U; // Raw STIMER value at the last credited read.
static uint64_t g_nsx_npu_tb_virtual  = 0U; // Microseconds accumulated so far.
static uint32_t g_nsx_npu_tb_rem      = 0U; // Conversion remainder, in (raw ticks * 1e6) mod hz.

//
// Rate cache: the CLKSEL the cached rate was derived for, and that rate. The
// pair is only ever updated together inside the critical section, so a
// reader can never pair a new CLKSEL with a stale rate.
//
static uint32_t g_nsx_npu_tb_rate_clksel = STIMER_STCFG_CLKSEL_NOCLK;
static uint32_t g_nsx_npu_tb_rate_hz     = 0U;

#ifdef ATOMIQ11X_FPGA
//
// Cache for nsx_npu_tb_fpga_calibrate_hfrc_root_hz(): populated at most once,
// by nsx_npu_timebase_init(), and only read afterwards (including by the
// per-tick nsx_npu_tb_source_hz() path via nsx_npu_tb_hfrc_root_hz()) so that
// the one-time calibration busy-wait never repeats. 0 means "not calibrated
// yet" -- either CLKMGR's HFRC report has been usable so far, or the tap live
// at the last init attempt wasn't HFRC-derived (see
// nsx_npu_tb_fpga_calibrate_hfrc_root_hz()). Deliberately not reset by
// nsx_npu_timebase_deinit(): the FPGA's core clock does not change within a
// boot, so a calibration already obtained stays valid across a deinit/reinit
// cycle and re-measuring would only re-pay the busy-wait for the same answer.
//
static uint32_t g_nsx_npu_tb_fpga_hfrc_root_hz = 0U;
#endif

//*****************************************************************************
//
// Source-rate derivation
//
//*****************************************************************************

//
// Query a root clock rate from CLKMGR, falling back to the HAL's own nominal
// macro when the clock is unconfigured or the query fails. Both the query and
// the fallback come from the HAL, so no frequency literal is introduced here.
//
static uint32_t nsx_npu_tb_clk_hz(am_hal_clkmgr_clock_id_e eClockId, uint32_t ui32FallbackHz)
{
    uint32_t ui32Hz = 0U;

    if ((am_hal_clkmgr_clock_config_get(eClockId, &ui32Hz, NULL) != AM_HAL_STATUS_SUCCESS) ||
        (ui32Hz == 0U))
    {
        ui32Hz = ui32FallbackHz;
    }

    return ui32Hz;
}

//
// Root oscillator behind a CLKSEL tap, and the ratio the tap applies to it,
// expressed as root * num / den. The ratios are the ones implied by the
// register-map tap names against this part's HFRC (nominally 500 MHz) and the
// 32768 Hz LS crystal; the roots are queried from CLKMGR at runtime because
// the FPGA runs a different clock tree than silicon and any baked-in Hz
// literal here would be wrong by construction.
//
//   HFRC_7_8125MHZ  500 MHz / 64      XTAL_32KHZ  32768 / 1
//   HFRC_488_KHZ    500 MHz / 1024    XTAL_8KHZ   32768 / 4
//   HFRC_16MHZ      500 MHz / 31.25   XTAL_2KHZ   32768 / 16
//                                     XTAL_1KHZ   32768 / 32
//                                     XTAL_512HZ  32768 / 64
//   LFRC_1KHZ       1 kHz nominal (the register map itself says "uncalibrated")
//
// The HFRC_16MHZ ratio is not a power of two, which is unusual for a hardware
// divider and is the one entry here that the register map does not corroborate
// beyond the tap's name. This module never *selects* that tap; it only honours
// it when the application has, so an error there mis-scales the deadline of an
// application that chose it rather than affecting the default path. The
// CTIMER-fed taps (CTIMER0/1) run at whatever rate a timer this module does not
// own produces and are deliberately absent.
//
// Accuracy caveat for atomiq110_fpga_turbo. The FPGA image is far slower than
// silicon -- cmake/socs/facts/atomiq110.cmake records the turbo core at 25 MHz
// -- while am_hal_clkgen.h's ATOMIQ11X_FPGA block still carries the silicon
// numbers under a "TODO: check actual frequencies on FPGA". CLKMGR's HFRC
// query on this FPGA image reliably reports status=SUCCESS/hz=0 (the clock
// exists but is not calibrated), so the HFRC root-Hz lookup below
// (nsx_npu_tb_hfrc_root_hz()) does not fall back straight to the HAL's
// silicon-nominal HFRC macro when that happens: it uses a once-per-boot
// self-calibration on the FPGA instead (nsx_npu_tb_fpga_calibrate_hfrc_root_hz(),
// TODO note atop this file). The silicon-nominal macro remains the
// last-resort fallback if calibration itself cannot get a reading, and is
// still safe in that case -- an over-estimated tick rate under-reports
// elapsed time, so the deadline fires late rather than early, converting an
// infinite hang into a bounded failure without ever aborting a
// long-but-healthy inference early.
//
typedef struct
{
    uint32_t ui32ClkSel;
    uint32_t ui32Num;
    uint32_t ui32Den;
} nsx_npu_tb_tap_t;

static const nsx_npu_tb_tap_t g_nsx_npu_tb_hfrc_taps[] = {
    { STIMER_STCFG_CLKSEL_HFRC_7_8125MHZ, 1U,   64U },
    { STIMER_STCFG_CLKSEL_HFRC_488_KHZ,   1U, 1024U },
    { STIMER_STCFG_CLKSEL_HFRC_16MHZ,     4U,  125U }, // 1 / 31.25
};

static const nsx_npu_tb_tap_t g_nsx_npu_tb_xtal_taps[] = {
    { STIMER_STCFG_CLKSEL_XTAL_32KHZ, 1U,  1U },
    { STIMER_STCFG_CLKSEL_XTAL_8KHZ,  1U,  4U },
    { STIMER_STCFG_CLKSEL_XTAL_2KHZ,  1U, 16U },
    { STIMER_STCFG_CLKSEL_XTAL_1KHZ,  1U, 32U },
    { STIMER_STCFG_CLKSEL_XTAL_512HZ, 1U, 64U },
};

#define NSX_NPU_TB_LFRC_HZ (1000U)

static uint32_t nsx_npu_tb_scaled_hz(const nsx_npu_tb_tap_t *psTaps, uint32_t ui32Count,
                                     uint32_t ui32ClkSel, uint32_t ui32RootHz)
{
    for (uint32_t i = 0U; i < ui32Count; i++)
    {
        if (psTaps[i].ui32ClkSel == ui32ClkSel)
        {
            return (uint32_t)(((uint64_t)ui32RootHz * psTaps[i].ui32Num) / psTaps[i].ui32Den);
        }
    }

    return 0U;
}

#ifdef ATOMIQ11X_FPGA
//
// One-shot HFRC root calibration for atomiq110_fpga_turbo. Called only from
// nsx_npu_timebase_init() (see the TODO note atop this file for why this
// exists and when it should go away) -- never from the per-tick
// nsx_npu_tb_source_hz() path, since it busy-waits.
//
// Measures the tap STIMER is *currently* running on -- ui32LiveClkSel, whatever
// nsx_npu_timebase_init() has just settled on -- against the CPU cycle counter
// (DWT->CYCCNT), which free-runs at the FPGA's own core clock. That core
// clock is not a guess: g_ui32FPGAfreqMHz (am_hal_global.h) is the same
// vendor-maintained value am_bsp_low_power_init() passes to
// am_hal_global_FPGAfreqSet(), and that am_hal_itm.c's own SWO clock-divisor
// math already depends on, so it tracks whatever frequency the current FPGA
// SOF actually runs at -- 25 MHz today, though am_bsp.c's own commented-out
// 48 MHz / 12 MHz alternatives show that has moved before and may again.
//
// This is the standard remedy for an uncalibrated/unreliable clock report:
// measure the clock in question against a second clock of known frequency.
// Here HFRC is the uncalibrated clock and the CPU cycle counter -- run off
// the known-frequency FPGA core clock -- is the reference.
//
// The measured tap rate is converted back to an equivalent HFRC *root* rate
// using the same ratio table nsx_npu_tb_scaled_hz() applies, so a single
// calibration (performed against whichever HFRC tap happened to be live at
// init) stays correct if the application later retunes STIMER to a different
// HFRC tap: nsx_npu_tb_source_hz() rescales the cached root by that tap's own
// ratio on every subsequent call, exactly as it would a CLKMGR-reported root.
//
// Returns 0 if ui32LiveClkSel is not one of the recognised HFRC taps (nothing
// to calibrate against right now; the cache is left as-is for a future init
// attempt to try again), or if the calibration window sees no STIMER or DWT
// movement (e.g. STIMER is genuinely stopped) -- the caller's own liveness
// probe (nsx_npu_tb_counter_advances()) reports that condition through its
// normal path, so this function does not need to.
//
static uint32_t nsx_npu_tb_fpga_calibrate_hfrc_root_hz(uint32_t ui32LiveClkSel)
{
    uint32_t ui32Num = 0U;
    uint32_t ui32Den = 0U;

    for (uint32_t i = 0U; i < sizeof(g_nsx_npu_tb_hfrc_taps) / sizeof(g_nsx_npu_tb_hfrc_taps[0]); i++)
    {
        if (g_nsx_npu_tb_hfrc_taps[i].ui32ClkSel == ui32LiveClkSel)
        {
            ui32Num = g_nsx_npu_tb_hfrc_taps[i].ui32Num;
            ui32Den = g_nsx_npu_tb_hfrc_taps[i].ui32Den;
            break;
        }
    }

    if (ui32Den == 0U)
    {
        return 0U; // Live tap isn't HFRC-derived; nothing to calibrate here.
    }

    // DWT is architectural (CMSIS core_cm55.h), not an ambiqsuite HAL/BSP
    // symbol, so enabling it needs no HAL call and does not conflict with
    // one; both writes are idempotent if trace is already enabled.
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    const uint32_t ui32StimBefore = am_hal_stimer_counter_get();
    const uint32_t ui32CycBefore  = DWT->CYCCNT;

    am_hal_delay_us(NSX_NPU_TB_FPGA_CAL_WINDOW_US);

    const uint32_t ui32StimDelta = am_hal_stimer_counter_get() - ui32StimBefore;
    const uint32_t ui32CycDelta  = DWT->CYCCNT - ui32CycBefore;

    if ((ui32StimDelta == 0U) || (ui32CycDelta == 0U))
    {
        return 0U;
    }

    // Measured tap rate: tap_hz = stimer_ticks * cpu_hz / cpu_cycles. Done in
    // 64 bits since the numerator can reach ~4.3e9 * 25e6.
    const uint64_t ui64CpuHz = (uint64_t)g_ui32FPGAfreqMHz * 1000000U;
    const uint64_t ui64TapHz = ((uint64_t)ui32StimDelta * ui64CpuHz) / ui32CycDelta;

    // Back out the HFRC root this tap implies, using this same tap's ratio --
    // the inverse of what nsx_npu_tb_scaled_hz() will reapply on every call.
    return (uint32_t)((ui64TapHz * ui32Den) / ui32Num);
}
#endif // ATOMIQ11X_FPGA

//
// HFRC root rate for nsx_npu_tb_source_hz()'s HFRC-derived taps. Prefers
// CLKMGR's own report; if CLKMGR cannot supply one, uses the FPGA
// self-calibration cache when populated (see
// nsx_npu_tb_fpga_calibrate_hfrc_root_hz()), and only then falls back to the
// HAL's silicon-nominal HFRC macro.
//
static uint32_t nsx_npu_tb_hfrc_root_hz(void)
{
    uint32_t ui32Hz = 0U;

    if ((am_hal_clkmgr_clock_config_get(AM_HAL_CLKMGR_CLK_ID_HFRC, &ui32Hz, NULL) ==
         AM_HAL_STATUS_SUCCESS) &&
        (ui32Hz != 0U))
    {
        return ui32Hz;
    }

#ifdef ATOMIQ11X_FPGA
    if (g_nsx_npu_tb_fpga_hfrc_root_hz != 0U)
    {
        return g_nsx_npu_tb_fpga_hfrc_root_hz;
    }
#endif

    return AM_HAL_CLKMGR_HFRC_FREQ_ADJ_500MHZ;
}

//
// Map a CLKSEL field to a rate in Hz. Returns 0 for taps whose rate cannot be
// derived: NOCLK, and the CTIMER-fed taps.
//
static uint32_t nsx_npu_tb_source_hz(uint32_t ui32ClkSel)
{
    switch (ui32ClkSel)
    {
        case STIMER_STCFG_CLKSEL_HFRC_7_8125MHZ:
        case STIMER_STCFG_CLKSEL_HFRC_488_KHZ:
        case STIMER_STCFG_CLKSEL_HFRC_16MHZ:
            return nsx_npu_tb_scaled_hz(
                g_nsx_npu_tb_hfrc_taps,
                sizeof(g_nsx_npu_tb_hfrc_taps) / sizeof(g_nsx_npu_tb_hfrc_taps[0]),
                ui32ClkSel,
                nsx_npu_tb_hfrc_root_hz());

        case STIMER_STCFG_CLKSEL_XTAL_32KHZ:
        case STIMER_STCFG_CLKSEL_XTAL_8KHZ:
        case STIMER_STCFG_CLKSEL_XTAL_2KHZ:
        case STIMER_STCFG_CLKSEL_XTAL_1KHZ:
        case STIMER_STCFG_CLKSEL_XTAL_512HZ:
            return nsx_npu_tb_scaled_hz(
                g_nsx_npu_tb_xtal_taps,
                sizeof(g_nsx_npu_tb_xtal_taps) / sizeof(g_nsx_npu_tb_xtal_taps[0]),
                ui32ClkSel,
                nsx_npu_tb_clk_hz(AM_HAL_CLKMGR_CLK_ID_XTAL_LS, AM_HAL_CLKMGR_DEFAULT_XTAL_LS_FREQ_HZ));

        case STIMER_STCFG_CLKSEL_LFRC_1KHZ:
            return NSX_NPU_TB_LFRC_HZ;

        default:
            return 0U;
    }
}

static uint32_t nsx_npu_tb_live_clksel(void)
{
    return _FLD2VAL(STIMER_STCFG_CLKSEL, STIMER->STCFG);
}

//
// Whether STIMER's configuration register describes a counter that is
// clocked, thawed and released from reset -- the three conditions
// am_hal_stimer_is_running() checks, minus the HAL's private "configured
// through the HAL" flag. See nsx_npu_timebase_init() for why both are used.
//
static bool nsx_npu_tb_stcfg_running(uint32_t ui32Cfg)
{
    return (_FLD2VAL(STIMER_STCFG_CLKSEL, ui32Cfg) != STIMER_STCFG_CLKSEL_NOCLK) &&
           (_FLD2VAL(STIMER_STCFG_FREEZE, ui32Cfg) == STIMER_STCFG_FREEZE_THAW) &&
           (_FLD2VAL(STIMER_STCFG_CLEAR, ui32Cfg) == STIMER_STCFG_CLEAR_RUN);
}

//
// Busy-wait long enough for ~2 ticks at ui32Hz, clamped, and report whether the
// counter moved. Runs once, at init, off the inference path.
//
static bool nsx_npu_tb_counter_advances(uint32_t ui32Hz)
{
    uint32_t ui32ProbeUs = (2000000U / ui32Hz) + 1U;

    if (ui32ProbeUs < NSX_NPU_TB_PROBE_US_MIN)
    {
        ui32ProbeUs = NSX_NPU_TB_PROBE_US_MIN;
    }
    else if (ui32ProbeUs > NSX_NPU_TB_PROBE_US_MAX)
    {
        ui32ProbeUs = NSX_NPU_TB_PROBE_US_MAX;
    }

    const uint32_t ui32Before = am_hal_stimer_counter_get();
    am_hal_delay_us(ui32ProbeUs);

    return (am_hal_stimer_counter_get() != ui32Before);
}

//*****************************************************************************
//
// Bring-up
//
//*****************************************************************************

nsx_npu_timebase_status_e nsx_npu_timebase_init(void)
{
    if (g_nsx_npu_tb_status == NSX_NPU_TIMEBASE_ARMED)
    {
        return NSX_NPU_TIMEBASE_ARMED; // Keep the running accumulator.
    }

    uint32_t ui32Cfg = STIMER->STCFG;

    //
    // Only claim STIMER if nobody else is using it. A block that is already
    // clocked and running belongs to the application; stomping it would break
    // whatever it is timing, and reading its CLKSEL back is enough for us.
    //
    // "Running" is asked of the HAL first, since am_hal_stimer_is_running() is
    // the sanctioned test and it correctly refuses a FREEZE'd counter (a frozen
    // STIMER reads as clocked and released from reset, but does not advance).
    // The HAL's answer also depends on its private "configured through
    // am_hal_stimer_config()" flag, though, so a counter that firmware started
    // by writing STCFG directly reports as not running. The register fields are
    // consulted as a second opinion in that case, so such a counter is used
    // rather than reconfigured out from under its owner.
    //
    // A STIMER started here is intentionally left running by nsx_npu_deinit():
    // it is a free-running counter with no interrupt enabled, and stopping it
    // would invalidate any elapsed-time measurement the application may have
    // started against it in the meantime.
    //
    if (!am_hal_stimer_is_running() && !nsx_npu_tb_stcfg_running(ui32Cfg))
    {
        (void)am_hal_stimer_config(NSX_NPU_TB_STIMER_CFG);
        ui32Cfg = STIMER->STCFG;
    }

    const uint32_t ui32ClkSel = _FLD2VAL(STIMER_STCFG_CLKSEL, ui32Cfg);

#ifdef ATOMIQ11X_FPGA
    //
    // One-shot: only pay the calibration busy-wait the first time this image
    // is asked to arm with CLKMGR's HFRC report unusable. Every later
    // nsx_npu_tb_source_hz() call for an HFRC-derived tap -- including the one
    // immediately below -- picks the cached result up through
    // nsx_npu_tb_hfrc_root_hz() itself, so nothing further is needed here.
    //
    if (g_nsx_npu_tb_fpga_hfrc_root_hz == 0U)
    {
        uint32_t ui32ClkMgrHz = 0U;
        const bool bClkMgrUsable =
            (am_hal_clkmgr_clock_config_get(AM_HAL_CLKMGR_CLK_ID_HFRC, &ui32ClkMgrHz, NULL) ==
             AM_HAL_STATUS_SUCCESS) &&
            (ui32ClkMgrHz != 0U);

        if (!bClkMgrUsable)
        {
            g_nsx_npu_tb_fpga_hfrc_root_hz = nsx_npu_tb_fpga_calibrate_hfrc_root_hz(ui32ClkSel);
        }
    }
#endif

    const uint32_t ui32Hz     = nsx_npu_tb_source_hz(ui32ClkSel);
    if (ui32Hz == 0U)
    {
        //
        // The application runs STIMER from a tap whose rate this module cannot
        // derive (CTIMER0/1). Reconfiguring it would break whatever the
        // application is timing, so the timebase stays disarmed and the
        // condition is reported instead.
        //
        g_nsx_npu_tb_status = NSX_NPU_TIMEBASE_UNSUPPORTED_CLOCK;
        return g_nsx_npu_tb_status;
    }

    if (!nsx_npu_tb_counter_advances(ui32Hz))
    {
        g_nsx_npu_tb_status = NSX_NPU_TIMEBASE_COUNTER_STOPPED;
        return g_nsx_npu_tb_status;
    }

    const uint32_t ui32Level = am_hal_interrupt_master_disable();

    g_nsx_npu_tb_last_raw    = am_hal_stimer_counter_get();
    g_nsx_npu_tb_virtual     = 0U;
    g_nsx_npu_tb_rem         = 0U;
    g_nsx_npu_tb_rate_clksel = ui32ClkSel;
    g_nsx_npu_tb_rate_hz     = ui32Hz;
    g_nsx_npu_tb_status      = NSX_NPU_TIMEBASE_ARMED;

    am_hal_interrupt_master_set(ui32Level);

    return NSX_NPU_TIMEBASE_ARMED;
}

nsx_npu_timebase_status_e nsx_npu_timebase_status(void)
{
    return g_nsx_npu_tb_status;
}

void nsx_npu_timebase_deinit(void)
{
    //
    // Drop back to the pre-init state so the next nsx_npu_timebase_init() re-runs
    // rate derivation and the liveness probe instead of taking the ARMED
    // short-circuit. nsx_npu_deinit() leaves STIMER running, so between here and
    // the next init the application may retune or stop it; a stale ARMED status
    // would otherwise keep nsx_ethos_u_ticks() converting against a rate that no
    // longer applies and nsx_npu_timebase_status() asserting a timebase that is
    // no longer valid.
    //
    const uint32_t ui32Level = am_hal_interrupt_master_disable();

    g_nsx_npu_tb_status      = NSX_NPU_TIMEBASE_NOT_INITIALIZED;
    g_nsx_npu_tb_last_raw    = 0U;
    g_nsx_npu_tb_virtual     = 0U;
    g_nsx_npu_tb_rem         = 0U;
    g_nsx_npu_tb_rate_clksel = STIMER_STCFG_CLKSEL_NOCLK;
    g_nsx_npu_tb_rate_hz     = 0U;

    am_hal_interrupt_master_set(ui32Level);
}

//*****************************************************************************
//
// Strong overrides of the nsx-ethos-u-driver weak timebase hooks
//
//*****************************************************************************

//
// How many raw ticks to credit for a read of ui32Raw when the previous credited
// read was ui32Last, at ui32Hz. Also reports whether ui32Raw should become the
// new reference (false only for a stale read, which must not move the
// reference backwards).
//
// Three situations put ui32Raw behind ui32Last, and they must be told apart
// because the wrong call in one direction aborts a healthy inference:
//
//   1. The 32-bit counter wrapped. The true advance is the modular difference,
//      which is small when this hook is being polled and up to ~2^32 between
//      widely spaced calls.
//   2. Another owner cleared the counter (am_hal_stimer_counter_clear(), a
//      public HAL API and an established in-tree idiom). It restarted from 0,
//      so the true advance since the clear is at most ui32Raw.
//   3. This read was taken (outside the critical section, deliberately) and
//      then overtaken by another caller that read a later value and updated
//      ui32Last first. Nothing elapsed that has not already been credited.
//
// Case 3 is the only one where the shortfall is small (bounded by how long
// another caller can hold the counter value before its update lands), so a
// small shortfall is read as stale. Beyond that window the two remaining
// interpretations are both plausible and the smaller is credited: for a real
// wrap that under-reports by at most ui32Raw ticks (a few, when polled), and
// for a real clear it is exact. Under-reporting delays a timeout; it never
// fires one early, which is the property that matters.
//
static uint32_t nsx_npu_tb_raw_elapsed(uint32_t ui32Raw, uint32_t ui32Last, uint32_t ui32Hz,
                                       bool *pbAdvanceReference)
{
    *pbAdvanceReference = true;

    if (ui32Raw >= ui32Last)
    {
        return ui32Raw - ui32Last;
    }

    const uint32_t ui32Behind = ui32Last - ui32Raw;
    uint32_t ui32StaleWindow  = (uint32_t)(((uint64_t)ui32Hz * NSX_NPU_TB_STALE_WINDOW_MS) / 1000U);
    if (ui32StaleWindow == 0U)
    {
        ui32StaleWindow = 1U;
    }

    if (ui32Behind <= ui32StaleWindow)
    {
        *pbAdvanceReference = false;
        return 0U;
    }

    const uint32_t ui32IfWrapped = 0U - ui32Behind; // (ui32Raw - ui32Last) mod 2^32
    const uint32_t ui32IfCleared = ui32Raw;

    return (ui32IfCleared < ui32IfWrapped) ? ui32IfCleared : ui32IfWrapped;
}

uint64_t nsx_ethos_u_ticks(void)
{
    if (g_nsx_npu_tb_status != NSX_NPU_TIMEBASE_ARMED)
    {
        //
        // No timebase. Return the sentinel so the pair stays consistent -- the
        // contract asks for both hooks or neither, and reporting a live counter
        // alongside a zero rate is exactly the half-provided configuration it
        // warns against.
        //
        return 0U;
    }

    //
    // Everything slow happens here, before interrupts are masked: the triple
    // read of the always-on counter register, and -- only when the application
    // has moved STIMER to another tap since the last call -- the CLKMGR query
    // behind the new rate. The critical section below is a fixed handful of
    // ALU instructions with no loop, no wait and no peripheral access, which is
    // what keeps this hook cheap to poll and safe to call from any context.
    //
    const uint32_t ui32ClkSel = nsx_npu_tb_live_clksel();
    uint32_t ui32NewHz        = 0U;
    if (ui32ClkSel != g_nsx_npu_tb_rate_clksel)
    {
        ui32NewHz = nsx_npu_tb_source_hz(ui32ClkSel);
    }

    const uint32_t ui32Raw = am_hal_stimer_counter_get();

    const uint32_t ui32Level = am_hal_interrupt_master_disable();

    //
    // A raw delta may only be converted when one identified rate governed the
    // whole [last_raw, raw] interval: the tap live now must be the tap that was
    // live at the previous call, and its rate must be derivable.
    // g_nsx_npu_tb_rate_clksel tracks the tap seen at the previous call
    // whatever it was -- including NOCLK and the CTIMER feeds -- so a tap that
    // changed and changed back is still recognised as a gap rather than
    // crediting the away-time at the returning tap's rate.
    //
    const bool bStableRate =
        (ui32ClkSel == g_nsx_npu_tb_rate_clksel) && (g_nsx_npu_tb_rate_hz != 0U);

    if (bStableRate)
    {
        bool bAdvance;
        const uint32_t ui32Hz      = g_nsx_npu_tb_rate_hz;
        const uint32_t ui32Elapsed = nsx_npu_tb_raw_elapsed(ui32Raw, g_nsx_npu_tb_last_raw, ui32Hz, &bAdvance);

        if (bAdvance)
        {
            //
            // Exact rational conversion, carrying the remainder so repeated
            // small deltas do not each lose a fraction of a microsecond: the
            // wait loop calls this thousands of times per millisecond, and a
            // per-call truncation would otherwise stall the virtual clock.
            //
            const uint64_t ui64Num = ((uint64_t)ui32Elapsed * NSX_NPU_TB_VIRTUAL_HZ) + g_nsx_npu_tb_rem;

            g_nsx_npu_tb_virtual += ui64Num / ui32Hz;
            g_nsx_npu_tb_rem      = (uint32_t)(ui64Num % ui32Hz);
            g_nsx_npu_tb_last_raw = ui32Raw;
        }
    }
    else
    {
        //
        // The tap changed since the previous call, or it is one whose rate this
        // module cannot derive (NOCLK / CTIMER-fed). The ticks since the last
        // call span an unknown mix of rates -- or a rate this module does not
        // know at all -- so credit nothing and just re-anchor. The unaccounted
        // span is a single poll interval, which can only push the deadline
        // later, never earlier. (Crediting it at max(old, new) Hz would keep a
        // conservative lower bound instead of dropping it, but the wait loop
        // polls far faster than the deadline's resolution, so one interval of
        // virtual time is not worth the extra state.)
        //
        bool bAdvance;
        const uint32_t ui32WindowHz = (ui32NewHz != 0U) ? ui32NewHz : NSX_NPU_TB_VIRTUAL_HZ;
        (void)nsx_npu_tb_raw_elapsed(ui32Raw, g_nsx_npu_tb_last_raw, ui32WindowHz, &bAdvance);

        g_nsx_npu_tb_rate_clksel = ui32ClkSel;
        g_nsx_npu_tb_rate_hz     = ui32NewHz; // 0 when the tap has no derivable rate
        g_nsx_npu_tb_rem         = 0U;
        if (bAdvance)
        {
            g_nsx_npu_tb_last_raw = ui32Raw;
        }
    }

    const uint64_t ui64Ticks = g_nsx_npu_tb_virtual;

    am_hal_interrupt_master_set(ui32Level);

    //
    // Wrap detection is polled rather than interrupt-driven, which is sound
    // here: the driver's bounded wait calls this every loop iteration, far
    // faster than the ~2.4 h it takes a 488 kHz 32-bit counter to wrap, so no
    // wrap can be missed *during* a wait. A wrap missed between waits only
    // under-reports elapsed time -- a late timeout, never a spurious one -- and
    // the accumulated value stays monotonic either way.
    //
    // The `| 1` bias keeps the result out of the driver's no-timebase sentinel
    // at a cost of at most one microsecond of skew on any elapsed-time
    // difference.
    //
    return ui64Ticks | UINT64_C(1);
}

uint32_t nsx_ethos_u_ticks_per_ms(void)
{
    return (g_nsx_npu_tb_status == NSX_NPU_TIMEBASE_ARMED) ? NSX_NPU_TB_VIRTUAL_TICKS_PER_MS : 0U;
}
