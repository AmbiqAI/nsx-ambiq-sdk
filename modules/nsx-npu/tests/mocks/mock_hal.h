/**
 * @file mock_hal.h
 * @brief Test-control API for the mock HAL backing host-native
 *        nsx_npu_timebase.c unit tests. See mock_hal.c for the simulation
 *        model.
 */
#ifndef NSX_NPU_TESTS_MOCK_HAL_H
#define NSX_NPU_TESTS_MOCK_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "am_mcu_apollo.h" // for am_hal_clkmgr_clock_id_e

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset all mock state to its power-on-equivalent default.
 *
 * Call at the top of every test. Defaults: STIMER->STCFG = NOCLK (not
 * running), STIMER counter = 0, DWT CYCCNT = 0/disabled, CLKMGR reports
 * SUCCESS/0 Hz for both HFRC and XTAL_LS (the real, observed
 * atomiq110_fpga_turbo bug this module's calibration path exists for),
 * g_ui32FPGAfreqMHz = 25 (the FPGA image's documented turbo-core rate),
 * am_hal_stimer_is_running() reports false.
 */
void mock_hal_reset(void);

/** @brief Directly set the raw STIMER counter's value (does not touch STCFG). */
void mock_stimer_set_counter(uint32_t ui32Value);

/** @brief What am_hal_stimer_is_running() will report. */
void mock_stimer_set_hal_running(bool bRunning);

/**
 * @brief Configure the simulated hardware clock rates, in Hz, that drive
 *        mock_advance_us() / am_hal_delay_us(): how fast the mock STIMER
 *        counter and the mock DWT->CYCCNT free-run per wall-clock
 *        microsecond of simulated time. A rate of 0 means that counter never
 *        advances (used to simulate a stopped/untraced clock).
 */
void mock_set_rates_hz(uint32_t ui32StimerHz, uint32_t ui32CyccntHz);

/**
 * @brief Advance simulated time by ui32Us microseconds: the mock STIMER
 *        counter and DWT->CYCCNT (only if trace is enabled, matching real
 *        DWT semantics) advance by rate_hz * ui32Us / 1e6, fractional
 *        remainders carried across calls so no advance is lost to
 *        truncation. am_hal_delay_us() calls this internally, so any busy
 *        wait nsx_npu_timebase.c performs advances the simulated clocks
 *        exactly as real elapsed time would.
 */
void mock_advance_us(uint32_t ui32Us);

/**
 * @brief Configure what am_hal_clkmgr_clock_config_get() reports for a given
 *        clock id: ui32Status (AM_HAL_STATUS_SUCCESS or any nonzero failure
 *        code) and the Hz it writes back through the out-param.
 */
void mock_set_clkmgr(am_hal_clkmgr_clock_id_e eClockId, uint32_t ui32Status, uint32_t ui32Hz);

/** @brief Set g_ui32FPGAfreqMHz, the CPU core clock the calibration path trusts. */
void mock_set_fpga_freq_mhz(uint32_t ui32Mhz);

#ifdef __cplusplus
}
#endif

#endif // NSX_NPU_TESTS_MOCK_HAL_H
