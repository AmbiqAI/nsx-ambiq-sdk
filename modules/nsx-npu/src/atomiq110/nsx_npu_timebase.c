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
 * Contract points from `nsx_ethos_u.h` that shape the implementation below:
 *
 *  - `nsx_ethos_u_ticks()` must wrap only at 2^64. STIMER's counter is 32 bits,
 *    so it is software-extended here rather than returned raw.
 *  - `nsx_ethos_u_ticks()` must never return 0 while the timebase is running;
 *    0 is the driver's "no time source" sentinel. The returned value is biased
 *    with `| 1`, which the contract explicitly sanctions.
 *  - Supply both hooks or neither. Because the `| 1` bias defeats the driver's
 *    own two-zero-reads liveness probe, this file runs its own liveness check
 *    at init and reports `ticks_per_ms() == 0` (and `ticks() == 0`) when the
 *    counter is not advancing, so a dead clock degrades to upstream's unbounded
 *    wait instead of a 100%-CPU spin that never times out.
 *
 * @copyright Copyright (c) 2026, Ambiq Micro, Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include "am_mcu_apollo.h"

#include "nsx_ethos_u.h"
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
// Fixed hardware dividers behind each CLKSEL tap, expressed against the root
// oscillator so the rate can be derived from CLKMGR at runtime instead of
// hard-coding a frequency. The FPGA runs a different clock tree than silicon,
// so any baked-in Hz literal here would be wrong by construction; the divider
// ratios, being hardware, are not.
//
// Ratios are the ones implied by the register-map tap names against this
// part's ~500 MHz HFRC (500 MHz / 64 = 7.8125 MHz, 500 MHz / 1024 = 488 kHz)
// and against the 32768 Hz LS crystal (/4 = 8 kHz, /32 = 1 kHz, /64 = 512 Hz).
//
// Accuracy caveat for atomiq110_fpga_turbo. The FPGA image is far slower than
// silicon -- cmake/socs/facts/atomiq110.cmake records the turbo core at 25 MHz
// -- while am_hal_clkgen.h's ATOMIQ11X_FPGA block still carries the silicon
// numbers under a "TODO: check actual frequencies on FPGA". Hence the CLKMGR
// query: if CLKMGR knows the real rate we scale correctly, and if it does not,
// the HAL nominal makes us *over*-estimate the tick rate, so elapsed time is
// under-reported and the deadline fires late rather than early. Late is the
// safe direction -- the timeout still converts an infinite hang into a bounded
// failure, and a long-but-healthy inference is never aborted spuriously.
//
#define NSX_NPU_TB_HFRC_DIV_7M8125 (64U)
#define NSX_NPU_TB_HFRC_DIV_488K   (1024U)
#define NSX_NPU_TB_XTAL_DIV_8KHZ   (4U)
#define NSX_NPU_TB_XTAL_DIV_1KHZ   (32U)
#define NSX_NPU_TB_XTAL_DIV_512HZ  (64U)

// LFRC is nominal-only (the register map itself says "uncalibrated").
#define NSX_NPU_TB_LFRC_HZ (1000U)

//
// HFRC_16MHZ does not fit the "divider off the CLKMGR-reported HFRC root"
// model above: 500 MHz / 16 MHz = 31.25, non-integral, so there is no clean
// divisor to express it as. Unlike the other taps, atomiq110.h names this one
// with an absolute rate rather than a ratio
// (STIMER_STCFG_CLKSEL_HFRC_16MHZ = 11, "16 MHz from the HFRC clock
// divider"), so treat it the same way LFRC above is treated: a register-map-
// cited literal rather than a derived value.
//
// This is a reasonable literal to trust, not a guess: the tap is named for
// its rate, and while the SVD-derived comments elsewhere in this map are
// occasionally off (tap 4 says 8162 Hz where 32768/4 = 8192, a transposed
// digit this file already silently corrects via the divisor idiom), a
// transposition typo is far likelier on an odd number like that than on a
// round figure attached to a tap literally named HFRC_16MHZ. It also only
// feeds a watchdog-style deadline, not a measurement, so a few percent of
// error here would loosen or tighten a timeout, not silently mis-scale a
// reported result.
//
#define NSX_NPU_TB_HFRC_16MHZ_HZ (16000000U)

// Bounds on the liveness-probe busy wait, in microseconds. The probe waits for
// roughly two ticks of the detected rate; the floor keeps fast taps from
// probing a sub-tick interval, the ceiling keeps a misdetected slow tap from
// stalling init.
#define NSX_NPU_TB_PROBE_US_MIN (100U)
#define NSX_NPU_TB_PROBE_US_MAX (8000U)

//*****************************************************************************
//
// Module state
//
//*****************************************************************************

//
// Published tick rate. Zero means "no time source" -- the value the driver
// reads as "wait forever", and the state this module stays in until
// nsx_npu_timebase_init() has both configured and liveness-checked a counter.
//
static volatile uint32_t g_nsx_npu_tb_ticks_per_ms = 0U;

//
// 32-to-64-bit software extension of the STIMER counter. Written only inside
// the critical section in nsx_ethos_u_ticks() / nsx_npu_timebase_init().
//
static uint32_t g_nsx_npu_tb_last_raw = 0U;
static uint64_t g_nsx_npu_tb_high     = 0U;

//*****************************************************************************
//
// Helpers
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

static uint32_t nsx_npu_tb_hfrc_hz(void)
{
    return nsx_npu_tb_clk_hz(AM_HAL_CLKMGR_CLK_ID_HFRC, AM_HAL_CLKMGR_HFRC_FREQ_ADJ_500MHZ);
}

static uint32_t nsx_npu_tb_xtal_ls_hz(void)
{
    return nsx_npu_tb_clk_hz(AM_HAL_CLKMGR_CLK_ID_XTAL_LS, AM_HAL_CLKMGR_DEFAULT_XTAL_LS_FREQ_HZ);
}

//
// Map the CLKSEL field that STIMER is *actually* running on to a rate in Hz.
// Reading the field back rather than assuming NSX_NPU_TB_STIMER_CFG means an
// application that had already configured STIMER for its own purposes keeps
// its configuration and still gets a correctly scaled NPU deadline.
//
// Returns 0 for taps whose rate cannot be derived (NOCLK, and the CTIMER-fed
// taps, whose rate depends on a timer this module does not own). 0 propagates
// as "no time source", i.e. upstream's unbounded wait.
//
static uint32_t nsx_npu_tb_source_hz(uint32_t ui32ClkSel)
{
    switch (ui32ClkSel)
    {
        case STIMER_STCFG_CLKSEL_HFRC_7_8125MHZ:
            return nsx_npu_tb_hfrc_hz() / NSX_NPU_TB_HFRC_DIV_7M8125;

        case STIMER_STCFG_CLKSEL_HFRC_488_KHZ:
            return nsx_npu_tb_hfrc_hz() / NSX_NPU_TB_HFRC_DIV_488K;

        case STIMER_STCFG_CLKSEL_XTAL_32KHZ:
            return nsx_npu_tb_xtal_ls_hz();

        case STIMER_STCFG_CLKSEL_XTAL_8KHZ:
            return nsx_npu_tb_xtal_ls_hz() / NSX_NPU_TB_XTAL_DIV_8KHZ;

        case STIMER_STCFG_CLKSEL_XTAL_1KHZ:
            return nsx_npu_tb_xtal_ls_hz() / NSX_NPU_TB_XTAL_DIV_1KHZ;

        case STIMER_STCFG_CLKSEL_XTAL_512HZ:
            return nsx_npu_tb_xtal_ls_hz() / NSX_NPU_TB_XTAL_DIV_512HZ;

        case STIMER_STCFG_CLKSEL_LFRC_1KHZ:
            return NSX_NPU_TB_LFRC_HZ;

        case STIMER_STCFG_CLKSEL_HFRC_16MHZ:
            return NSX_NPU_TB_HFRC_16MHZ_HZ;

        default:
            return 0U;
    }
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

void nsx_npu_timebase_init(void)
{
    if (g_nsx_npu_tb_ticks_per_ms != 0U)
    {
        return;
    }

    const uint32_t ui32Cfg = STIMER->STCFG;
    uint32_t ui32ClkSel    = _FLD2VAL(STIMER_STCFG_CLKSEL, ui32Cfg);
    const bool bRunning    = (_FLD2VAL(STIMER_STCFG_CLEAR, ui32Cfg) == STIMER_STCFG_CLEAR_RUN);

    //
    // Only claim STIMER if nobody else is using it. A block that is already
    // clocked and running belongs to the application; stomping it would break
    // whatever it is timing, and reading its CLKSEL back is enough for us.
    //
    // Note that a STIMER started here is intentionally left running by
    // nsx_npu_deinit(): it is a free-running counter with no interrupt enabled,
    // and stopping it would invalidate any elapsed-time measurement the
    // application may have started against it in the meantime.
    //
    if ((ui32ClkSel == STIMER_STCFG_CLKSEL_NOCLK) || !bRunning)
    {
        (void)am_hal_stimer_config(NSX_NPU_TB_STIMER_CFG);
        ui32ClkSel = _FLD2VAL(STIMER_STCFG_CLKSEL, STIMER->STCFG);
    }

    const uint32_t ui32Hz = nsx_npu_tb_source_hz(ui32ClkSel);
    if (ui32Hz == 0U)
    {
        return; // Rate not derivable -> stay at "no time source".
    }

    if (!nsx_npu_tb_counter_advances(ui32Hz))
    {
        return; // Counter is stopped -> stay at "no time source".
    }

    //
    // Round to nearest rather than truncating: the contract notes that an
    // integral ticks-per-ms makes a 32768 Hz source fire ~2.4% early, and
    // rounding halves that error for free. A source below 500 Hz rounds to 0,
    // which correctly reads as "no usable time source".
    //
    const uint32_t ui32Level = am_hal_interrupt_master_disable();

    g_nsx_npu_tb_last_raw     = am_hal_stimer_counter_get();
    g_nsx_npu_tb_high         = 0U;
    g_nsx_npu_tb_ticks_per_ms = (ui32Hz + 500U) / 1000U;

    am_hal_interrupt_master_set(ui32Level);
}

//*****************************************************************************
//
// Strong overrides of the nsx-ethos-u-driver weak timebase hooks
//
//*****************************************************************************

uint64_t nsx_ethos_u_ticks(void)
{
    if (g_nsx_npu_tb_ticks_per_ms == 0U)
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
    // am_hal_stimer_counter_get() is an uninvasive read of a free-running
    // peripheral register with no shared software state of its own, so it
    // does not need to be inside the critical section below -- only the
    // compare-and-update against g_nsx_npu_tb_last_raw / g_nsx_npu_tb_high
    // does, because that state is shared with any other caller of this hook.
    // Reading the counter first keeps PRIMASK masked for only the handful of
    // instructions that touch that shared state, instead of also covering the
    // register access, which matters here: this hook is polled every
    // iteration of the driver's bounded wait with no WFI/WFE by default, so a
    // critical section here is on the hot path for the whole span of the
    // wait, not a one-off cost.
    //
    const uint32_t ui32Raw = am_hal_stimer_counter_get();

    const uint32_t ui32Level = am_hal_interrupt_master_disable();

    if (ui32Raw < g_nsx_npu_tb_last_raw)
    {
        g_nsx_npu_tb_high += (UINT64_C(1) << 32);
    }
    g_nsx_npu_tb_last_raw = ui32Raw;

    const uint64_t ui64Ticks = g_nsx_npu_tb_high + (uint64_t)ui32Raw;

    am_hal_interrupt_master_set(ui32Level);

    //
    // Wrap detection is polled rather than interrupt-driven, which is sound
    // here: the driver's bounded wait calls this every loop iteration, far
    // faster than the ~2.4 h it takes a 488 kHz 32-bit counter to wrap, so no
    // wrap can be missed *during* a wait. A wrap missed between waits only
    // under-reports elapsed time -- a late timeout, never a spurious one -- and
    // the extended value stays monotonic either way.
    //
    // The `| 1` bias keeps the result out of the driver's no-timebase sentinel
    // at a cost of at most one tick of skew on any elapsed-time difference.
    //
    return ui64Ticks | UINT64_C(1);
}

uint32_t nsx_ethos_u_ticks_per_ms(void)
{
    return g_nsx_npu_tb_ticks_per_ms;
}
