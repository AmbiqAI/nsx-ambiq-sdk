#include <assert.h>
#include <string.h>
#include "am_mcu_apollo.h"
#include "nsx_power.h"

uint32_t SystemCoreClock = 1234567u;
power_regs power;
#if !defined(TEST_APOLLO3)
static bool wrong_hardware, missing_ack;
#endif
static unsigned transitions, initializations;
static uint32_t transition_status, init_status;
static bool available = true, write_availability = true;
static bool last_high;

#if defined(TEST_APOLLO3)
uint32_t am_hal_burst_mode_initialize(am_hal_burst_avail_e *value) {
    ++initializations;
    if (write_availability) *value = available ? AM_HAL_BURST_AVAIL : AM_HAL_BURST_NOTAVAIL;
    return init_status;
}
uint32_t am_hal_burst_mode_enable(am_hal_burst_mode_e *value) {
    ++transitions;
    last_high = true;
    *value = AM_HAL_BURST_MODE;
    return transition_status;
}
uint32_t am_hal_burst_mode_disable(am_hal_burst_mode_e *value) {
    ++transitions;
    last_high = false;
    *value = AM_HAL_NORMAL_MODE;
    return transition_status;
}
#else
uint32_t am_hal_pwrctrl_mcu_mode_select(am_hal_pwrctrl_mcu_mode_e mode) {
    assert(mode == AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE || mode == AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER);
    last_high = mode == AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE;
    ++transitions;
    power.MCUPERFREQ_b.MCUPERFACK=!missing_ack;
    power.MCUPERFREQ_b.MCUPERFSTATUS=wrong_hardware ? 99u : (unsigned)mode;
    return transition_status;
}
#endif

static void select_ok(nsx_power_perf_mode_t mode, bool high) {
    unsigned count = transitions;
    assert(nsx_power_set_performance_mode(mode) == NSX_STATUS_SUCCESS);
    assert(transitions == count + 1);
    assert(last_high == high);
#if defined(TEST_APOLLO3) || defined(APOLLO4_FPGA)
    assert(SystemCoreClock == 1234567u);
#else
    assert(SystemCoreClock == (high ? 192000000u : 96000000u));
#endif
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const char *scenario = argv[1];
    if (!strcmp(scenario, "low")) select_ok(NSX_POWER_PERF_LOW, false);
    else if (!strcmp(scenario, "high")) select_ok(NSX_POWER_PERF_HIGH, true);
    else if (!strcmp(scenario, "max")) select_ok(NSX_POWER_PERF_MAX, true);
    else if (!strcmp(scenario, "repeat")) {
        select_ok(NSX_POWER_PERF_HIGH, true);
        select_ok(NSX_POWER_PERF_HIGH, true);
        select_ok(NSX_POWER_PERF_LOW, false);
        select_ok(NSX_POWER_PERF_MAX, true);
#if defined(TEST_APOLLO3)
        assert(initializations == 1);
#endif
    } else if (!strcmp(scenario, "invalid") || !strcmp(scenario, "invalid-after-init")) {
        if (!strcmp(scenario, "invalid-after-init")) select_ok(NSX_POWER_PERF_LOW, false);
        unsigned before_init = initializations, before_transition = transitions;
        const nsx_power_perf_mode_t modes[] = {NSX_POWER_PERF_NOT_SET, (nsx_power_perf_mode_t)3, (nsx_power_perf_mode_t)-2};
        for (unsigned i = 0; i < sizeof(modes)/sizeof(modes[0]); ++i)
            assert(nsx_power_set_performance_mode(modes[i]) == NSX_STATUS_INVALID_CONFIG);
        assert(initializations == before_init && transitions == before_transition);
    } else if (!strcmp(scenario, "enable-fails") || !strcmp(scenario, "disable-fails")) {
        nsx_power_perf_mode_t mode = !strcmp(scenario, "enable-fails") ? NSX_POWER_PERF_HIGH : NSX_POWER_PERF_LOW;
        transition_status = 17u;
        assert(nsx_power_set_performance_mode(mode) == 17u);
        assert(transitions == 1);
        assert(last_high == (mode == NSX_POWER_PERF_HIGH));
        transition_status = 0;
        select_ok(mode, mode == NSX_POWER_PERF_HIGH);
#if defined(TEST_APOLLO3)
        assert(initializations == 1);
#endif
#if defined(TEST_APOLLO3)
    } else if (!strcmp(scenario, "init-fails") || !strcmp(scenario, "init-no-output")) {
        init_status = 23u;
        write_availability = strcmp(scenario, "init-no-output") != 0;
        assert(nsx_power_set_performance_mode(NSX_POWER_PERF_HIGH) == 23u);
        assert(initializations == 1 && transitions == 0);
        init_status = 0;
        write_availability = true;
        select_ok(NSX_POWER_PERF_HIGH, true);
        assert(initializations == 2);
    } else if (!strcmp(scenario, "unavailable")) {
        available = false;
        assert(nsx_power_set_performance_mode(NSX_POWER_PERF_HIGH) == (uint32_t)NSX_STATUS_FAILURE);
        assert(initializations == 1 && transitions == 0);
        available = true;
        select_ok(NSX_POWER_PERF_LOW, false);
        assert(initializations == 2);
#endif
#if !defined(TEST_APOLLO3)
    } else if (!strcmp(scenario,"wrong-hardware") || !strcmp(scenario,"missing-ack")) {
        wrong_hardware=!strcmp(scenario,"wrong-hardware"); missing_ack=!strcmp(scenario,"missing-ack");
        for (unsigned i=0;i<2;++i) {
            assert(nsx_power_set_performance_mode(NSX_POWER_PERF_HIGH) != 0);
            assert(SystemCoreClock == 1234567u);
        }
        wrong_hardware=missing_ack=false;
        select_ok(NSX_POWER_PERF_HIGH,true);
#endif
    } else assert(!"unknown scenario");
    return 0;
}
