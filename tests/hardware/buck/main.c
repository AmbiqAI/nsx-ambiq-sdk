#include "am_mcu_apollo.h"
#include "nsx_power_buck.h"
#include "result_reporter.h"
#include "unity.h"

#define WINDOW_TICKS 8192u

static const nsx_power_buck_session_t session = {
    .rails = NSX_BUCK_RAIL(NSX_BUCK_VDDC) | NSX_BUCK_RAIL(NSX_BUCK_VDDF) |
             NSX_BUCK_RAIL(NSX_BUCK_VDDS) | NSX_BUCK_RAIL(NSX_BUCK_VDDC_LV),
    .timer = {[NSX_BUCK_VDDC] = 7, [NSX_BUCK_VDDF] = 8,
              [NSX_BUCK_VDDS] = 9, [NSX_BUCK_VDDC_LV] = 10},
};
static bool owns_session;

void setUp(void) {}
void tearDown(void) {
    if (owns_session) {
        TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_OK, nsx_power_buck_stop(&session));
        owns_session = false;
    }
}

static nsx_buck_supply_t observed_supply(void) {
    switch (PWRCTRL->VRSTATUS_b.SIMOBUCKST) {
        case PWRCTRL_VRSTATUS_SIMOBUCKST_OFF: return NSX_BUCK_SUPPLY_OFF;
        case PWRCTRL_VRSTATUS_SIMOBUCKST_LP: return NSX_BUCK_SUPPLY_LOW_POWER;
        case PWRCTRL_VRSTATUS_SIMOBUCKST_ACT: return NSX_BUCK_SUPPLY_ACTIVE;
        default: return NSX_BUCK_SUPPLY_UNKNOWN;
    }
}

static void measure_window(void) {
    nsx_power_buck_sample_t sample;
    nsx_buck_supply_t before = observed_supply();
    TEST_ASSERT_EQUAL_UINT32(session.rails, nsx_power_buck_rails());
    nsx_buck_status_t status = nsx_power_buck_start(&session);
    owns_session = (status == NSX_BUCK_OK || status == NSX_BUCK_CLEANUP_REQUIRED);
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_OK, status);
    nsx_power_buck_session_t other = session;
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_BUSY, nsx_power_buck_start(&other));
    uint32_t start = am_hal_stimer_counter_get();
    // Bound a stalled crystal independently of the STIMER being exercised.
    unsigned poll;
    for (poll = 0; poll < 4000000u; ++poll) {
        if ((uint32_t)(am_hal_stimer_counter_get() - start) >= WINDOW_TICKS) break;
    }
    TEST_ASSERT_TRUE_MESSAGE(poll < 4000000u, "STIMER stalled");
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_OK, nsx_power_buck_read(&session, &sample));
    TEST_ASSERT_EQUAL_UINT32(before, sample.start_modes.main_supply);
    TEST_ASSERT_EQUAL_UINT32(observed_supply(), sample.end_modes.main_supply);
    TEST_ASSERT_EQUAL_UINT32(before, sample.end_modes.main_supply);
    TEST_ASSERT_EQUAL_UINT32(32768, sample.tick_hz);
    TEST_ASSERT_TRUE(sample.elapsed_ticks >= WINDOW_TICKS && sample.elapsed_ticks < WINDOW_TICKS * 2);
    TEST_ASSERT_EQUAL_UINT32(session.rails, sample.rails);
    UnityPrint("supply="); UnityPrintNumberUnsigned(before);
    UnityPrint(" cpu="); UnityPrintNumberUnsigned(sample.start_modes.cpu_mode);
    UnityPrint(" ticks="); UnityPrintNumberUnsigned(sample.elapsed_ticks);
    UnityPrint(" pulses=");
    uint32_t any = 0;
    for (unsigned rail = 0; rail < NSX_BUCK_RAIL_COUNT; ++rail) {
        if (session.rails & NSX_BUCK_RAIL(rail)) {
            UnityPrintNumberUnsigned(sample.pulses[rail]); UnityPrint(" ");
            any |= sample.pulses[rail];
        }
    }
    UNITY_OUTPUT_CHAR('\n');
    if (before == NSX_BUCK_SUPPLY_OFF) {
        TEST_ASSERT_EQUAL_UINT32(0, any);
    } else {
        TEST_ASSERT_NOT_EQUAL(0, any);
    }
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_OK, nsx_power_buck_stop(&session));
    owns_session = false;
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_NOT_ACTIVE, nsx_power_buck_read(&session, &sample));
}

static void initial_supply_window(void) {
    am_hal_stimer_config(AM_HAL_STIMER_XTAL_32KHZ);
    measure_window();
}

static void enabled_supply_window(void) {
    if (observed_supply() == NSX_BUCK_SUPPLY_OFF) {
        TEST_ASSERT_EQUAL_UINT32(0, am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_SIMOBUCK_INIT, NULL));
    }
    TEST_ASSERT_NOT_EQUAL(NSX_BUCK_SUPPLY_OFF, observed_supply());
    measure_window();
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(initial_supply_window);
    RUN_TEST(enabled_supply_window);
    nsx_test_finish();
}
