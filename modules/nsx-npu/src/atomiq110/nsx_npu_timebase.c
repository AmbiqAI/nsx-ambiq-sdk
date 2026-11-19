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
 *    not converted: it is dropped and the reference re-anchored. Changes are detected by sampling CLKSEL.
 *    A switch away and back entirely between samples cannot be observed.
 *
 * Converting per call, from the live CLKSEL, is what makes the deadline immune
 * to the application reconfiguring STIMER: whether it moves the tap before
 * `nsx_npu_init()`, between inferences, or in the middle of one, an observed change
 * re-anchors the counter without altering the span computed from `ticks_per_ms()`. A cached rate cannot
 * give that guarantee -- a tap change from 488 kHz to 7.8 MHz against a cached
 * 488 kHz over-reports elapsed time 16x and aborts a healthy inference.
 *
 * The other two hazards a shared counter presents are handled in the delta
 * accounting:
 *
 *  - A 32-bit wrap and `am_hal_stimer_counter_clear()` both show up as the raw
 *    value going backwards. Credit only ticks since zero, which elapsed
 *    under either interpretation.
 *    Whichever it really was, the error is an under-report -- a late timeout,
 *    never a spurious one.
 *  - The counter read itself is kept *outside* the critical section (it is a
 *    triple read of a slow always-on register, and this hook is polled in a
 *    tight loop for the whole inference). A read that is overtaken by another
 *    caller's update is discarded.
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
// Prefer HFRC when starting an idle STIMER; it runs whenever the FPGA core runs.
#define NSX_NPU_TB_STIMER_CFG (AM_HAL_STIMER_HFRC_488KHZ | AM_HAL_STIMER_CFG_RUN)

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
// Virtual-clock accumulator. These fields are read-modify-written together
// inside the critical section in nsx_ethos_u_ticks(); nothing else touches
// them once armed.
//
static uint32_t g_nsx_npu_tb_last_raw = 0U; // Raw STIMER value at the last credited read.
static uint64_t g_nsx_npu_tb_virtual  = 0U; // Microseconds accumulated so far.
static uint32_t g_nsx_npu_tb_rem      = 0U; // Conversion remainder, in (raw ticks * 1e6) mod hz.

// A nested reader can publish a newer sample while the raw counter is read.
static volatile uint32_t g_nsx_npu_tb_revision;

//
// Rate cache: the CLKSEL the cached rate was derived for, and that rate. The
// pair is only ever updated together inside the critical section, so a
// reader can never pair a new CLKSEL with a stale rate.
//
static volatile uint32_t g_nsx_npu_tb_rate_clksel = STIMER_STCFG_CLKSEL_NOCLK;
static uint32_t g_nsx_npu_tb_rate_hz     = 0U;

//*****************************************************************************
//
// Source-rate derivation
//
//*****************************************************************************

#ifdef ATOMIQ11X_FPGA
// AmbiqSuite's AT110 stimer_test_cases.c speed_test() specifies these
// FPGA roots. CLKMGR can return zero or the nominal silicon frequency.
#define NSX_NPU_TB_FPGA_LS_HZ 40916U
#define NSX_NPU_TB_LFRC_HZ ((NSX_NPU_TB_FPGA_LS_HZ + 31U) / 32U)

static uint32_t nsx_npu_tb_root_hz(am_hal_clkmgr_clock_id_e clock)
{
    return clock == AM_HAL_CLKMGR_CLK_ID_HFRC ? 50000000U : NSX_NPU_TB_FPGA_LS_HZ;
}
#else
#define NSX_NPU_TB_LFRC_HZ 1000U

static uint32_t nsx_npu_tb_root_hz(am_hal_clkmgr_clock_id_e clock)
{
    uint32_t hz = 0U;
    if (am_hal_clkmgr_clock_config_get(clock, &hz, NULL) == AM_HAL_STATUS_SUCCESS && hz != 0U)
    {
        return hz;
    }
    return clock == AM_HAL_CLKMGR_CLK_ID_HFRC ? AM_HAL_CLKMGR_HFRC_FREQ_ADJ_500MHZ
                                              : AM_HAL_CLKMGR_DEFAULT_XTAL_LS_FREQ_HZ;
}
#endif

//
// Root oscillator behind a CLKSEL tap, and the ratio the tap applies to it,
// expressed as root * num / den. The ratios are the ones implied by the
// register-map tap names against this part's HFRC (nominally 500 MHz) and the
// 32768 Hz LS crystal; the FPGA uses the same dividers with its own roots.
//
//   HFRC_7_8125MHZ  500 MHz / 64      XTAL_32KHZ  32768 / 1
//   HFRC_488_KHZ    500 MHz / 1024    XTAL_8KHZ   32768 / 4
//                                    XTAL_2KHZ   32768 / 16
//                                     XTAL_1KHZ   32768 / 32
//                                     XTAL_512HZ  32768 / 64
//   LFRC_1KHZ       1 kHz nominal (the register map itself says "uncalibrated")
//
// The current register map has no selector 11 (formerly HFRC_16MHZ).
// Unsupported selectors and CTIMER-fed taps have no known rate here.
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
};

static const nsx_npu_tb_tap_t g_nsx_npu_tb_xtal_taps[] = {
    { STIMER_STCFG_CLKSEL_XTAL_32KHZ, 1U,  1U },
    { STIMER_STCFG_CLKSEL_XTAL_8KHZ,  1U,  4U },
    { STIMER_STCFG_CLKSEL_XTAL_2KHZ,  1U, 16U },
    { STIMER_STCFG_CLKSEL_XTAL_1KHZ,  1U, 32U },
    { STIMER_STCFG_CLKSEL_XTAL_512HZ, 1U, 64U },
};

static uint32_t nsx_npu_tb_scaled_hz(const nsx_npu_tb_tap_t *psTaps, uint32_t ui32Count,
                                     uint32_t ui32ClkSel, uint32_t ui32RootHz)
{
    for (uint32_t i = 0U; i < ui32Count; i++)
    {
        if (psTaps[i].ui32ClkSel == ui32ClkSel)
        {
            uint64_t numerator = (uint64_t)ui32RootHz * psTaps[i].ui32Num;
#ifdef ATOMIQ11X_FPGA
            numerator += psTaps[i].ui32Den - 1U; // Round fractional FPGA rates toward a later deadline.
#endif
            return (uint32_t)(numerator / psTaps[i].ui32Den);
        }
    }

    return 0U;
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
            return nsx_npu_tb_scaled_hz(
                g_nsx_npu_tb_hfrc_taps,
                sizeof(g_nsx_npu_tb_hfrc_taps) / sizeof(g_nsx_npu_tb_hfrc_taps[0]),
                ui32ClkSel,
                nsx_npu_tb_root_hz(AM_HAL_CLKMGR_CLK_ID_HFRC));

        case STIMER_STCFG_CLKSEL_XTAL_32KHZ:
        case STIMER_STCFG_CLKSEL_XTAL_8KHZ:
        case STIMER_STCFG_CLKSEL_XTAL_2KHZ:
        case STIMER_STCFG_CLKSEL_XTAL_1KHZ:
        case STIMER_STCFG_CLKSEL_XTAL_512HZ:
            return nsx_npu_tb_scaled_hz(
                g_nsx_npu_tb_xtal_taps,
                sizeof(g_nsx_npu_tb_xtal_taps) / sizeof(g_nsx_npu_tb_xtal_taps[0]),
                ui32ClkSel,
                nsx_npu_tb_root_hz(AM_HAL_CLKMGR_CLK_ID_XTAL_LS));

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
    ++g_nsx_npu_tb_revision;
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

    ++g_nsx_npu_tb_revision;
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

uint64_t nsx_ethos_u_ticks(void)
{
    const uint32_t revision = g_nsx_npu_tb_revision;
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
    // register reads and arithmetic, with no loop or wait.
    //
    const uint32_t ui32PreviousClkSel = g_nsx_npu_tb_rate_clksel;
    const uint32_t ui32ClkSel = nsx_npu_tb_live_clksel();
    uint32_t ui32NewHz        = 0U;
    if (ui32ClkSel != ui32PreviousClkSel)
    {
        ui32NewHz = nsx_npu_tb_source_hz(ui32ClkSel);
    }

    const uint32_t ui32Raw = am_hal_stimer_counter_get();

    const uint32_t ui32Level = am_hal_interrupt_master_disable();

    if (revision != g_nsx_npu_tb_revision)
    {
        // A nested call published a newer sample, possibly after a counter clear.
        const uint64_t ticks = g_nsx_npu_tb_status == NSX_NPU_TIMEBASE_ARMED
                             ? (g_nsx_npu_tb_virtual | UINT64_C(1)) : 0U;
        am_hal_interrupt_master_set(ui32Level);
        return ticks;
    }

    ++g_nsx_npu_tb_revision;
    if (ui32ClkSel != nsx_npu_tb_live_clksel())
    {
        // The selector changed during the raw read. Anchor at the next coherent sample.
        g_nsx_npu_tb_rate_clksel = STIMER_STCFG_CLKSEL_NOCLK;
        g_nsx_npu_tb_rate_hz = 0U;
    }
    else if (ui32ClkSel == ui32PreviousClkSel && g_nsx_npu_tb_rate_hz != 0U)
    {
        // For a backwards count, only the ticks since zero are certain for both a clear and a wrap.
        const uint32_t elapsed = ui32Raw < g_nsx_npu_tb_last_raw ? ui32Raw : ui32Raw - g_nsx_npu_tb_last_raw;
        const uint64_t numerator = (uint64_t)elapsed * NSX_NPU_TB_VIRTUAL_HZ + g_nsx_npu_tb_rem;
        g_nsx_npu_tb_virtual += numerator / g_nsx_npu_tb_rate_hz;
        g_nsx_npu_tb_rem = (uint32_t)(numerator % g_nsx_npu_tb_rate_hz);
        g_nsx_npu_tb_last_raw = ui32Raw;
    }
    else
    {
        // Time between different taps cannot be converted at a single rate.
        g_nsx_npu_tb_rate_clksel = ui32ClkSel;
        g_nsx_npu_tb_rate_hz = ui32NewHz;
        g_nsx_npu_tb_rem = 0U;
        g_nsx_npu_tb_last_raw = ui32Raw;
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
