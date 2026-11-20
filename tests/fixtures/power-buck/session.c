#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "nsx_power_buck_internal.h"

static uint32_t now, hz = 32768, counts[16], running;
static unsigned starts, fail_start, fail_stop, gated_timer;
static bool skip_touch, freeze_on_start;
static nsx_buck_modes_t modes = {NSX_BUCK_SUPPLY_OFF, NSX_BUCK_SUPPLY_UNKNOWN, NSX_BUCK_CPU_LP};
static nsx_power_buck_session_t cfg = {
    .rails = NSX_BUCK_RAIL(NSX_BUCK_VDDC) | NSX_BUCK_RAIL(NSX_BUCK_VDDF),
    .timer = {[NSX_BUCK_VDDC] = 7, [NSX_BUCK_VDDF] = 8}
};

uint32_t nsx_power_buck_rails(void) { return 15; }
nsx_buck_status_t nsx_buck_hw_check(nsx_buck_rail_t rail, uint8_t timer) {
    (void)rail;
    return timer >= 5 && timer < 14 ? NSX_BUCK_OK : NSX_BUCK_INVALID;
}
nsx_buck_status_t nsx_buck_hw_start(nsx_buck_rail_t rail, uint8_t timer, bool *touched) {
    (void)rail;
    ++starts;
    assert(nsx_power_buck_stop(&cfg) == NSX_BUCK_BUSY);
    if (timer == fail_start && skip_touch) return NSX_BUCK_BUSY;
    *touched = true;
    running |= 1u << timer;
    if (freeze_on_start) hz = 0;
    return timer == fail_start ? NSX_BUCK_HAL_ERROR : NSX_BUCK_OK;
}
nsx_buck_status_t nsx_buck_hw_stop(uint8_t timer) {
    assert(running & (1u << timer));
    if (timer == fail_stop) return NSX_BUCK_HAL_ERROR;
    running &= ~(1u << timer);
    return NSX_BUCK_OK;
}
bool nsx_buck_hw_count(uint8_t timer, uint32_t *count) {
    if (!(running & (1u << timer)) || timer == gated_timer) return false;
    *count = counts[timer];
    return true;
}
uint32_t nsx_buck_hw_time(uint32_t *tick) { *tick = now; return hz; }
nsx_buck_modes_t nsx_buck_hw_modes(void) { return modes; }

int main(void) {
    nsx_power_buck_sample_t sample = {0};
    nsx_power_buck_session_t other = cfg;
    assert(nsx_power_buck_start(NULL) == NSX_BUCK_INVALID);
    other.rails = 0;
    assert(nsx_power_buck_start(&other) == NSX_BUCK_INVALID);
    other.rails = NSX_BUCK_RAIL(NSX_BUCK_BLE);
    assert(nsx_power_buck_start(&other) == NSX_BUCK_UNSUPPORTED);
    other = cfg;
    other.timer[1] = 7;
    assert(nsx_power_buck_start(&other) == NSX_BUCK_INVALID);
    other.timer[1] = 15;
    assert(nsx_power_buck_start(&other) == NSX_BUCK_INVALID);
    hz = 0;
    assert(nsx_power_buck_start(&cfg) == NSX_BUCK_NO_TIMEBASE);
    assert(starts == 0);
    hz = 32768;

    now = counts[7] = UINT32_MAX - 4;
    assert(nsx_power_buck_start(&cfg) == NSX_BUCK_OK);
    assert(nsx_power_buck_start(&other) == NSX_BUCK_BUSY);
    assert(nsx_power_buck_read(&other, &sample) == NSX_BUCK_NOT_ACTIVE);
    assert(nsx_power_buck_stop(&other) == NSX_BUCK_NOT_ACTIVE);
    now += 20;
    counts[7] += 18;
    modes.main_supply = NSX_BUCK_SUPPLY_ACTIVE;
    assert(nsx_power_buck_read(&cfg, &sample) == NSX_BUCK_OK);
    assert(sample.elapsed_ticks == 20 && sample.tick_hz == hz);
    assert(sample.pulses[0] == 18 && sample.pulses[1] == 0 && sample.rails == cfg.rails);
    assert(sample.start_modes.main_supply == NSX_BUCK_SUPPLY_OFF);
    assert(sample.end_modes.main_supply == NSX_BUCK_SUPPLY_ACTIVE);
    hz = 1024;
    assert(nsx_power_buck_read(&cfg, &sample) == NSX_BUCK_NO_TIMEBASE);
    assert(sample.tick_hz == 32768 && sample.elapsed_ticks == 20);
    hz = 32768;
    assert(nsx_power_buck_read(&cfg, &sample) == NSX_BUCK_NO_TIMEBASE);
    assert(nsx_power_buck_stop(&cfg) == NSX_BUCK_OK && !running);
    assert(nsx_power_buck_stop(&cfg) == NSX_BUCK_NOT_ACTIVE);

    assert(nsx_power_buck_start(&cfg) == NSX_BUCK_OK);
    unsigned char saved_sample[sizeof(sample)];
    memcpy(saved_sample, &sample, sizeof(sample));
    ++now;
    ++counts[7];
    gated_timer = 8;
    assert(nsx_power_buck_read(&cfg, &sample) == NSX_BUCK_INTERRUPTED);
    assert(memcmp(saved_sample, &sample, sizeof(sample)) == 0);
    gated_timer = 0;
    assert(nsx_power_buck_read(&cfg, &sample) == NSX_BUCK_INTERRUPTED);
    assert(nsx_power_buck_stop(&cfg) == NSX_BUCK_OK && !running);
    gated_timer = 8;
    assert(nsx_power_buck_start(&cfg) == NSX_BUCK_INTERRUPTED && !running);
    gated_timer = 0;
    assert(nsx_power_buck_start(&cfg) == NSX_BUCK_OK);
    assert(nsx_power_buck_read(&cfg, &sample) == NSX_BUCK_OK);
    assert(nsx_power_buck_stop(&cfg) == NSX_BUCK_OK && !running);

    fail_start = 8;
    skip_touch = true;
    assert(nsx_power_buck_start(&cfg) == NSX_BUCK_BUSY && !running);
    skip_touch = false;
    fail_stop = 7;
    assert(nsx_power_buck_start(&cfg) == NSX_BUCK_CLEANUP_REQUIRED && running == (1u << 7));
    assert(nsx_power_buck_start(&other) == NSX_BUCK_BUSY);
    assert(nsx_power_buck_read(&cfg, &sample) == NSX_BUCK_CLEANUP_REQUIRED);
    fail_stop = 0;
    assert(nsx_power_buck_stop(&cfg) == NSX_BUCK_OK && !running);
    fail_start = 0;
    freeze_on_start = true;
    assert(nsx_power_buck_start(&cfg) == NSX_BUCK_NO_TIMEBASE && !running);
}
