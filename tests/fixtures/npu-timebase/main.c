#include <assert.h>
#include "am_mcu_apollo.h"
#include "nsx_ethos_u.h"
#include "nsx_npu_timebase.h"

mock_stimer_t timer = { STIMER_STCFG_CLKSEL_XTAL_32KHZ };
static uint32_t raw, switch_during_read;
static bool running = true, nested_clear;

bool am_hal_stimer_is_running(void) { return true; }
uint32_t am_hal_stimer_config(uint32_t cfg) { timer.STCFG = cfg; return cfg; }
void am_hal_delay_us(uint32_t us) { raw += running && us > 0; }
uint32_t am_hal_interrupt_master_disable(void) { return 0; }
void am_hal_interrupt_master_set(uint32_t level) { (void)level; }
uint32_t am_hal_stimer_counter_get(void)
{
    if (switch_during_read) {
        timer.STCFG = switch_during_read;
        switch_during_read = 0;
    }
    uint32_t sample = raw;
    if (nested_clear) {
        nested_clear = false;
        raw = 0;
        (void)nsx_ethos_u_ticks();
    }
    return sample;
}

#ifndef ATOMIQ11X_FPGA
// Silicon must honor the reported roots even when they differ from the defaults.
uint32_t am_hal_clkmgr_clock_config_get(am_hal_clkmgr_clock_id_e id, uint32_t *hz, void *source)
{
    (void)source;
    *hz = id == AM_HAL_CLKMGR_CLK_ID_HFRC ? 256000000U : 64000U;
    return AM_HAL_STATUS_SUCCESS;
}
#endif

int main(void)
{
#ifdef ATOMIQ11X_FPGA
    // Raw counts per 100 ms from AmbiqSuite's AT110 STIMER speed test.
    const uint32_t counts[] = {78125, 4883, 4092, 1023, 256, 128, 64, 128};
#else
    const uint32_t counts[] = {400000, 25000, 6400, 1600, 400, 200, 100, 100};
#endif
    assert(nsx_npu_timebase_status() == NSX_NPU_TIMEBASE_NOT_INITIALIZED);
    assert(nsx_ethos_u_ticks() == 0);
    assert(nsx_ethos_u_ticks_per_ms() == 0);
    assert(nsx_npu_timebase_init() == NSX_NPU_TIMEBASE_ARMED);
    assert(nsx_ethos_u_ticks_per_ms() == 1000);
    for (unsigned i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        timer.STCFG = 1 + i;
        uint64_t start = nsx_ethos_u_ticks();
        raw += counts[i];
        uint64_t elapsed = nsx_ethos_u_ticks() - start;
        assert(elapsed >= 99000 && elapsed <= 101000);
    }

    uint64_t before = nsx_ethos_u_ticks();
    switch_during_read = STIMER_STCFG_CLKSEL_HFRC_7_8125MHZ;
    raw += counts[0];
    assert(nsx_ethos_u_ticks() == before);
    assert(nsx_ethos_u_ticks() == before);
    raw += counts[0];
    assert(nsx_ethos_u_ticks() - before == 100000);

    // An interrupt clears and samples the same tap after our outer raw read.
    before = nsx_ethos_u_ticks();
    nested_clear = true;
    ++raw;
    assert(nsx_ethos_u_ticks() == before);
    raw += counts[0];
    assert(nsx_ethos_u_ticks() - before == 100000);

    // Repeated init must preserve both elapsed time and any ticks since the last read.
    before = nsx_ethos_u_ticks();
    raw += counts[0];
    assert(nsx_npu_timebase_init() == NSX_NPU_TIMEBASE_ARMED);
    assert(nsx_ethos_u_ticks() == before + 100000);

    nsx_npu_timebase_deinit();
    raw += counts[0];
    assert(nsx_npu_timebase_status() == NSX_NPU_TIMEBASE_NOT_INITIALIZED);
    assert(nsx_ethos_u_ticks() == 0);
    assert(nsx_ethos_u_ticks_per_ms() == 0);

    running = false;
    assert(nsx_npu_timebase_init() == NSX_NPU_TIMEBASE_COUNTER_STOPPED);
    assert(nsx_ethos_u_ticks() == 0);
    assert(nsx_ethos_u_ticks_per_ms() == 0);

    running = true;
    assert(nsx_npu_timebase_init() == NSX_NPU_TIMEBASE_ARMED);
    assert(nsx_npu_timebase_status() == NSX_NPU_TIMEBASE_ARMED);
    assert(nsx_ethos_u_ticks_per_ms() == 1000);
    before = nsx_ethos_u_ticks();
    raw += counts[0];
    assert(nsx_ethos_u_ticks() - before == 100000);
    return 0;
}
