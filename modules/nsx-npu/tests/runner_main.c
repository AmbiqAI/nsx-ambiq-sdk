/**
 * @file runner_main.c
 * @brief Hand-written Unity test runner for nsx_npu_timebase_tests.c.
 *
 * modules/nsx-core/tests' Unity/.ini convention (test_file/test_list) isn't
 * wired to any generator or build in this repo (no CMakeLists, no vendored
 * Unity, no runner script were found for it), so this suite ships its own
 * small, directly-runnable main() instead of depending on one. The .ini file
 * alongside this one lists the same tests for whenever/if that convention
 * gains real tooling.
 */
#include "unity/unity.h"

void nsx_npu_timebase_tests_pre_test_hook(void);
void nsx_npu_timebase_tests_post_test_hook(void);

void nsx_npu_timebase_test_fpga_calibration_matches_real_elapsed_time(void);
void nsx_npu_timebase_test_fpga_calibration_regression_pin(void);
void nsx_npu_timebase_test_without_calibration_reproduces_original_bug(void);
void nsx_npu_timebase_test_healthy_clkmgr_bypasses_calibration(void);
void nsx_npu_timebase_test_ticks_per_ms_is_1000_once_armed(void);
void nsx_npu_timebase_test_ticks_zero_when_not_armed(void);
void nsx_npu_timebase_test_deinit_requires_reinit(void);
void nsx_npu_timebase_test_counter_stopped_detected(void);
void nsx_npu_timebase_test_unsupported_clock_tap_reported(void);
void nsx_npu_timebase_test_repeated_init_keeps_running_accumulator(void);

void setUp(void)
{
    nsx_npu_timebase_tests_pre_test_hook();
}

void tearDown(void)
{
    nsx_npu_timebase_tests_post_test_hook();
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(nsx_npu_timebase_test_fpga_calibration_matches_real_elapsed_time);
    RUN_TEST(nsx_npu_timebase_test_fpga_calibration_regression_pin);
    RUN_TEST(nsx_npu_timebase_test_without_calibration_reproduces_original_bug);
    RUN_TEST(nsx_npu_timebase_test_healthy_clkmgr_bypasses_calibration);
    RUN_TEST(nsx_npu_timebase_test_ticks_per_ms_is_1000_once_armed);
    RUN_TEST(nsx_npu_timebase_test_ticks_zero_when_not_armed);
    RUN_TEST(nsx_npu_timebase_test_deinit_requires_reinit);
    RUN_TEST(nsx_npu_timebase_test_counter_stopped_detected);
    RUN_TEST(nsx_npu_timebase_test_unsupported_clock_tap_reported);
    RUN_TEST(nsx_npu_timebase_test_repeated_init_keeps_running_accumulator);

    return UNITY_END();
}
