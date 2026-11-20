#include "am_mcu_apollo.h"
#include "nsx_power_buck.h"
#include "result_reporter.h"
#include "unity.h"

static const nsx_power_buck_session_t session = {
    .rails = NSX_BUCK_RAIL(NSX_BUCK_CPU) | NSX_BUCK_RAIL(NSX_BUCK_MEMORY) | NSX_BUCK_RAIL(NSX_BUCK_BLE),
    .timer = {[NSX_BUCK_CPU] = 4, [NSX_BUCK_MEMORY] = 5, [NSX_BUCK_BLE] = 6},
};

static bool owns_session;
static void *ble;

void setUp(void) {}
void tearDown(void) {
    nsx_buck_status_t stop = owns_session ? nsx_power_buck_stop(&session) : NSX_BUCK_OK;
    owns_session = false;
    if (ble) {
        uint32_t off = am_hal_ble_power_control(ble, AM_HAL_BLE_POWER_OFF);
        uint32_t deinit = am_hal_ble_deinitialize(ble);
        ble = NULL;
        TEST_ASSERT_EQUAL_UINT32(0, off);
        TEST_ASSERT_EQUAL_UINT32(0, deinit);
    }
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_OK, stop);
}

static void measure(uint32_t window) {
    nsx_power_buck_sample_t sample;
    nsx_buck_supply_t main_supply = am_hal_pwrctrl_simobuck_enabled_check() ? NSX_BUCK_SUPPLY_ON : NSX_BUCK_SUPPLY_OFF;
    nsx_buck_supply_t ble_supply = am_hal_pwrctrl_blebuck_enabled_check() ? NSX_BUCK_SUPPLY_ON : NSX_BUCK_SUPPLY_OFF;
    nsx_buck_cpu_t cpu = am_hal_burst_mode_status() == AM_HAL_BURST_MODE ? NSX_BUCK_CPU_BURST : NSX_BUCK_CPU_NORMAL;
    TEST_ASSERT_EQUAL_UINT32(session.rails, nsx_power_buck_rails());
    nsx_buck_status_t status = nsx_power_buck_start(&session);
    owns_session = (status == NSX_BUCK_OK || status == NSX_BUCK_CLEANUP_REQUIRED);
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_OK, status);
    uint32_t start = am_hal_stimer_counter_get();
    unsigned poll;
    for (poll = 0; poll < 64000000u; ++poll) {
        if ((uint32_t)(am_hal_stimer_counter_get() - start) >= window) break;
    }
    TEST_ASSERT_TRUE_MESSAGE(poll < 64000000u, "STIMER stalled");
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_OK, nsx_power_buck_read(&session, &sample));
    TEST_ASSERT_EQUAL_UINT32(session.rails, sample.rails);
    TEST_ASSERT_EQUAL_UINT32(32768, sample.tick_hz);
    TEST_ASSERT_TRUE(sample.elapsed_ticks >= window && sample.elapsed_ticks < window + 512);
    TEST_ASSERT_EQUAL_UINT32(cpu, sample.start_modes.cpu_mode);
    TEST_ASSERT_EQUAL_UINT32(cpu, sample.end_modes.cpu_mode);
    TEST_ASSERT_EQUAL_UINT32(main_supply, sample.start_modes.main_supply);
    TEST_ASSERT_EQUAL_UINT32(main_supply, sample.end_modes.main_supply);
    TEST_ASSERT_EQUAL_UINT32(ble_supply, sample.start_modes.ble_supply);
    TEST_ASSERT_EQUAL_UINT32(ble_supply, sample.end_modes.ble_supply);
    UnityPrint("ticks="); UnityPrintNumberUnsigned(sample.elapsed_ticks);
    UnityPrint(" main="); UnityPrintNumberUnsigned(main_supply);
    UnityPrint(" ble="); UnityPrintNumberUnsigned(ble_supply);
    UnityPrint(" cpu/memory/ble pulses=");
    for (unsigned i = NSX_BUCK_CPU; i <= NSX_BUCK_BLE; ++i) {
        UnityPrintNumberUnsigned(sample.pulses[i]); UNITY_OUTPUT_CHAR(' ');
    }
    UNITY_OUTPUT_CHAR('\n');
    TEST_ASSERT_EQUAL_UINT32(main_supply == NSX_BUCK_SUPPLY_ON,
                             (sample.pulses[NSX_BUCK_CPU] | sample.pulses[NSX_BUCK_MEMORY]) != 0);
    TEST_ASSERT_EQUAL_UINT32(ble_supply == NSX_BUCK_SUPPLY_ON, sample.pulses[NSX_BUCK_BLE] != 0);
    if (window == 3 * 32768 && main_supply == NSX_BUCK_SUPPLY_ON) {
        TEST_ASSERT_TRUE_MESSAGE(sample.pulses[NSX_BUCK_CPU] > UINT16_MAX ||
                                 sample.pulses[NSX_BUCK_MEMORY] > UINT16_MAX,
                                 "Linked high half was not observed");
    }
    am_hal_ctimer_stop(session.timer[NSX_BUCK_CPU], AM_HAL_CTIMER_BOTH);
    sample.rails = 0;
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_INTERRUPTED, nsx_power_buck_read(&session, &sample));
    TEST_ASSERT_EQUAL_UINT32(0, sample.rails);
    am_hal_ctimer_start(session.timer[NSX_BUCK_CPU], AM_HAL_CTIMER_BOTH);
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_INTERRUPTED, nsx_power_buck_read(&session, &sample));
}

static void initial_supplies(void) {
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_XTAL_START, NULL));
    am_hal_stimer_config(AM_HAL_STIMER_XTAL_32KHZ);
    measure(8192);
}

static void ble_powered(void) {
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_ble_initialize(0, &ble));
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_ble_power_control(ble, AM_HAL_BLE_POWER_ACTIVE));
    TEST_ASSERT_TRUE(am_hal_pwrctrl_blebuck_enabled_check());
    // Keep the linked counters running long enough to exercise the high halves.
    measure(3 * 32768);
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(initial_supplies);
    RUN_TEST(ble_powered);
    nsx_test_finish();
}
