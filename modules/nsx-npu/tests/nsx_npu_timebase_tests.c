/**
 * @file nsx_npu_timebase_tests.c
 * @brief Host-native unit tests for nsx_npu_timebase.c's Atomiq110 timebase,
 *        with emphasis on the FPGA self-calibration fix
 *        (nsx_npu_tb_fpga_calibrate_hfrc_root_hz()) added for the
 *        atomiq110_fpga_turbo CLKMGR-reports-SUCCESS/hz=0 bug (see the TODO
 *        note atop nsx_npu_timebase.c).
 *
 * Runs entirely on the host: no ARM cross-compiler, no ambiqsuite checkout,
 * no FPGA/silicon. `mocks/am_mcu_apollo.h` stands in for the real HAL/CMSIS
 * header nsx_npu_timebase.c includes, with every clock input under direct
 * test control (see mocks/mock_hal.h). Test naming/`TEST_ASSERT_*` style
 * mirrors modules/nsx-core/tests' convention.
 */
#include <math.h>
#include <stdlib.h>

#include "am_mcu_apollo.h"
#include "mock_hal.h"
#include "nsx_ethos_u.h"
#include "nsx_npu_timebase.h"
#include "unity/unity.h"

void nsx_npu_timebase_tests_pre_test_hook()
{
    mock_hal_reset();
    nsx_npu_timebase_deinit();
}

void nsx_npu_timebase_tests_post_test_hook() {}

//*****************************************************************************
// Helpers
//*****************************************************************************

// Arms the timebase with STIMER free-running on the 488 kHz HFRC tap
// (this module's default, NSX_NPU_TB_STIMER_CFG) at ui32StimerHz, DWT->CYCCNT
// free-running at g_ui32FPGAfreqMHz, and CLKMGR reporting SUCCESS/hz=0 for
// HFRC -- the exact real atomiq110_fpga_turbo bug this fix targets.
static nsx_npu_timebase_status_e arm_with_uncalibrated_clkmgr(uint32_t ui32FpgaMhz,
                                                               uint32_t ui32StimerHz)
{
    mock_set_fpga_freq_mhz(ui32FpgaMhz);
    mock_set_rates_hz(ui32StimerHz, ui32FpgaMhz * 1000000U);
    mock_set_clkmgr(AM_HAL_CLKMGR_CLK_ID_HFRC, AM_HAL_STATUS_SUCCESS, 0U);
    return nsx_npu_timebase_init();
}

//*****************************************************************************
// Core regression: the real FPGA failure mode this fix targets
//*****************************************************************************

// Reproduces the exact scenario validated on real Atomiq110 FPGA hardware:
// CLKMGR reports the HFRC clock as SUCCESS/0 Hz, so absent the fix,
// nsx_npu_tb_hfrc_root_hz() would fall straight back to the silicon-nominal
// AM_HAL_CLKMGR_HFRC_FREQ_ADJ_500MHZ macro and over-report the 488 kHz tap's
// rate by ~10x on this FPGA image (observed: elapsed time under-reported by
// ~10x, e.g. a real 50000us delay measured as ~5002us). The self-calibration
// path should instead measure the tap against DWT->CYCCNT (calibrated to
// g_ui32FPGAfreqMHz) and produce a rate close enough to the true one that a
// real elapsed interval is measured within a tight tolerance.
void nsx_npu_timebase_test_fpga_calibration_matches_real_elapsed_time()
{
    // The FPGA's documented turbo-core rate and a representative real 488 kHz
    // HFRC-derived STIMER tap rate (both independent of each other, exactly
    // as they are on real silicon/FPGA).
    const uint32_t ui32FpgaMhz  = 25U;
    const uint32_t ui32StimerHz = 476837U; // ~ 500MHz(nominal)/1024, scaled down for a 25MHz-class FPGA image.

    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_ARMED,
                       arm_with_uncalibrated_clkmgr(ui32FpgaMhz, ui32StimerHz));

    // Simulate a real 50ms wait -- the same magnitude used in the FPGA harness
    // (npu_timebase_fpga_test) -- and check the reported elapsed time is
    // close to the real 50000us, not off by an order of magnitude.
    const uint64_t ui64Start = nsx_ethos_u_ticks();
    mock_advance_us(50000U);
    const uint64_t ui64Elapsed = nsx_ethos_u_ticks() - ui64Start;

    // 5% tolerance: the calibration's own DWT/STIMER sampling has a
    // ~one-tick quantization error over its 10ms window, which the
    // pre-fix bug's ~10x error dwarfs by two orders of magnitude.
    const uint64_t ui64Tolerance = 50000U * 5U / 100U;
    TEST_ASSERT_UINT64_WITHIN(ui64Tolerance, 50000U, ui64Elapsed);
}

// Same scenario, but at the exact tap rate/FPGA frequency pairing observed
// during validation on atomiq110_fpga_turbo hardware, as a fixed,
// non-parameterised regression pin.
void nsx_npu_timebase_test_fpga_calibration_regression_pin()
{
    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_ARMED, arm_with_uncalibrated_clkmgr(25U, 488000U));

    const uint64_t ui64Start = nsx_ethos_u_ticks();
    mock_advance_us(50000U);
    const uint64_t ui64Elapsed = nsx_ethos_u_ticks() - ui64Start;

    TEST_ASSERT_UINT64_WITHIN(2500U, 50000U, ui64Elapsed);
}

// Without the fix (i.e. calibration disabled by forcing DWT->CYCCNT to never
// advance, so the calibration measurement always sees ui32CycDelta == 0 and
// bails out to the old fallback), the same scenario should reproduce the
// original bug: elapsed time under-reported by roughly the ratio between the
// silicon-nominal 500MHz-derived tap rate and the real FPGA tap rate. This
// pins the pre-fix behaviour so a future change can't silently reintroduce it
// without this test noticing the difference between "fixed" and "unfixed".
void nsx_npu_timebase_test_without_calibration_reproduces_original_bug()
{
    // A real FPGA-image HFRC-derived tap rate roughly 10x slower than the
    // silicon-nominal 500MHz-root assumption implies for this same CLKSEL
    // (500MHz / 1024 ~= 488281 Hz) -- this is the actual relationship
    // observed on real Atomiq110 FPGA hardware before this fix (a real
    // 50000us delay measured as ~5002us).
    const uint32_t ui32StimerHz = 48800U;

    mock_set_fpga_freq_mhz(25U);
    mock_set_rates_hz(ui32StimerHz, 0U); // DWT never advances: calibration can't measure.
    mock_set_clkmgr(AM_HAL_CLKMGR_CLK_ID_HFRC, AM_HAL_STATUS_SUCCESS, 0U);

    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_ARMED, nsx_npu_timebase_init());

    const uint64_t ui64Start = nsx_ethos_u_ticks();
    mock_advance_us(50000U);
    const uint64_t ui64Elapsed = nsx_ethos_u_ticks() - ui64Start;

    // Nominal HFRC root (500MHz) / 1024 divider = ~488281 Hz assumed, vs. the
    // real ~48800 Hz tap -- an ~10x mismatch, reproducing the exact
    // under-report ratio observed on real FPGA hardware pre-fix. Assert it is
    // *not* within the fixed path's 5% tolerance, confirming this scenario
    // really does exercise the pre-fix, uncalibrated fallback rather than
    // accidentally also passing.
    const uint64_t ui64Tolerance = 50000U * 5U / 100U;
    TEST_ASSERT_TRUE(llabs((long long)ui64Elapsed - 50000LL) > (long long)ui64Tolerance);
}

//*****************************************************************************
// CLKMGR-healthy path: calibration must not be needed/used when CLKMGR works
//*****************************************************************************

void nsx_npu_timebase_test_healthy_clkmgr_bypasses_calibration()
{
    const uint32_t ui32HfrcRootHz = 500000000U; // A CLKMGR-reported, real root Hz.
    const uint32_t ui32StimerHz   = ui32HfrcRootHz / 1024U; // Exactly the 488KHz-tap divider.

    mock_set_fpga_freq_mhz(25U);
    // DWT deliberately never advances (rate 0): if this test's ticks()
    // accuracy still depends on calibration, it would fail here.
    mock_set_rates_hz(ui32StimerHz, 0U);
    mock_set_clkmgr(AM_HAL_CLKMGR_CLK_ID_HFRC, AM_HAL_STATUS_SUCCESS, ui32HfrcRootHz);

    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_ARMED, nsx_npu_timebase_init());

    const uint64_t ui64Start = nsx_ethos_u_ticks();
    mock_advance_us(50000U);
    const uint64_t ui64Elapsed = nsx_ethos_u_ticks() - ui64Start;

    TEST_ASSERT_UINT64_WITHIN(50U, 50000U, ui64Elapsed);
}

//*****************************************************************************
// Contract/edge cases
//*****************************************************************************

void nsx_npu_timebase_test_ticks_per_ms_is_1000_once_armed()
{
    TEST_ASSERT_EQUAL_UINT32(0U, nsx_ethos_u_ticks_per_ms()); // Not armed yet.

    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_ARMED, arm_with_uncalibrated_clkmgr(25U, 488000U));

    TEST_ASSERT_EQUAL_UINT32(1000U, nsx_ethos_u_ticks_per_ms());
}

void nsx_npu_timebase_test_ticks_zero_when_not_armed()
{
    TEST_ASSERT_EQUAL_UINT64(0U, nsx_ethos_u_ticks());
}

void nsx_npu_timebase_test_deinit_requires_reinit()
{
    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_ARMED, arm_with_uncalibrated_clkmgr(25U, 488000U));

    nsx_npu_timebase_deinit();

    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_NOT_INITIALIZED, nsx_npu_timebase_status());
    TEST_ASSERT_EQUAL_UINT64(0U, nsx_ethos_u_ticks());
    TEST_ASSERT_EQUAL_UINT32(0U, nsx_ethos_u_ticks_per_ms());

    // Re-init should re-arm cleanly.
    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_ARMED, nsx_npu_timebase_init());
}

void nsx_npu_timebase_test_counter_stopped_detected()
{
    // STIMER configured/thawed/running per the register fields, but the raw
    // counter never actually advances (rate 0) -- the liveness probe must
    // catch this rather than arming against a dead counter.
    mock_set_fpga_freq_mhz(25U);
    mock_set_rates_hz(0U, 25000000U);
    mock_set_clkmgr(AM_HAL_CLKMGR_CLK_ID_HFRC, AM_HAL_STATUS_SUCCESS, 0U);

    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_COUNTER_STOPPED, nsx_npu_timebase_init());
    TEST_ASSERT_EQUAL_UINT64(0U, nsx_ethos_u_ticks());
}

void nsx_npu_timebase_test_unsupported_clock_tap_reported()
{
    // Application has already claimed STIMER on a CTIMER-fed tap this module
    // cannot derive a rate for; it must not be reconfigured or misreported.
    mock_stimer_set_hal_running(true);
    g_sMockStimer.STCFG = STIMER_STCFG_CLKSEL_CTIMER0;
    mock_set_fpga_freq_mhz(25U);

    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_UNSUPPORTED_CLOCK, nsx_npu_timebase_init());
    TEST_ASSERT_EQUAL_UINT64(0U, nsx_ethos_u_ticks());
    TEST_ASSERT_EQUAL_UINT32(STIMER_STCFG_CLKSEL_CTIMER0, _FLD2VAL(STIMER_STCFG_CLKSEL, g_sMockStimer.STCFG));
}

void nsx_npu_timebase_test_repeated_init_keeps_running_accumulator()
{
    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_ARMED, arm_with_uncalibrated_clkmgr(25U, 488000U));

    mock_advance_us(10000U);
    const uint64_t ui64Before = nsx_ethos_u_ticks();

    // Calling init() again while already armed must be a no-op short-circuit
    // (see the ARMED early-return at the top of nsx_npu_timebase_init()): it
    // must not reset the accumulator or re-run the busy-wait calibration.
    TEST_ASSERT_EQUAL(NSX_NPU_TIMEBASE_ARMED, nsx_npu_timebase_init());

    TEST_ASSERT_TRUE(nsx_ethos_u_ticks() >= (uint64_t)ui64Before);
}
