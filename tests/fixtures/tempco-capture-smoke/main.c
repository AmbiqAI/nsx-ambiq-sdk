#include "nsx_system.h"
#include "unity.h"
#include "result_reporter.h"
/* Inspect the real private helper without adding a public diagnostic API. */
#include "nsx_power_tempco.c"

static volatile uint32_t observed;
static uint32_t statuses[2], elapsed[2];
static bool sleeping[2], powered[2];
static uint32_t power_status[2];
void setUp(void) {}
void tearDown(void) {
    TEST_ASSERT_EQUAL_UINT32(0, nsx_power_tempco_disable());
}
static void observe(nsx_timer_config_t *cfg) {
    uint32_t start=am_hal_stimer_counter_get();
    nsx_power_tempco_callback(cfg);
    if (observed<2) {
        elapsed[observed]=am_hal_stimer_counter_get()-start;
        statuses[observed]=s_nsx_power_tempco.last_handler_status;
        sleeping[observed]=s_nsx_power_tempco.suspended;
        power_status[observed]=am_hal_pwrctrl_periph_enabled(AM_HAL_PWRCTRL_PERIPH_ADC, &powered[observed]);
        ++observed;
    }
}
static void capture_and_resume(void) {
    nsx_system_config_t system={.enable_cache=true,.perf_mode=NSX_PERF_LOW,
                                .debug={.transport=NSX_DEBUG_ITM}};
    TEST_ASSERT_EQUAL_UINT32(0,nsx_system_init(&system));
    am_hal_stimer_config(STIMER_STCFG_CLEAR_Msk | STIMER_STCFG_FREEZE_Msk);
    am_hal_stimer_counter_clear();
    am_hal_stimer_config(AM_HAL_STIMER_HFRC_6MHZ);
    s_nsx_power_tempco_timer.callback=observe;
    TEST_ASSERT_EQUAL_UINT32(0,nsx_power_tempco_enable());
    __enable_irq();
    uint32_t start=am_hal_stimer_counter_get();
    while(observed<2 && (uint32_t)(am_hal_stimer_counter_get()-start)<25u*6000000u) {}
    TEST_ASSERT_EQUAL_UINT32(2,observed);
    for(unsigned i=0;i<2;++i) {
        UnityPrint("capture_ticks_6mhz="); UnityPrintNumberUnsigned(elapsed[i]); UnityPrint("\n");
        TEST_ASSERT_EQUAL_UINT32(0,statuses[i]);
        TEST_ASSERT_TRUE(sleeping[i]);
        TEST_ASSERT_EQUAL_UINT32(0,power_status[i]);
        TEST_ASSERT_FALSE(powered[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(2,s_nsx_power_tempco.callback_count);
    for(unsigned i=0;i<AM_HAL_TEMPCO_NUMSAMPLES;++i) {
        TEST_ASSERT_EQUAL_UINT32(NSX_POWERTEMPCO_ADC_TEMPERATURE_SLOT,s_nsx_power_tempco.samples[i].ui32Slot);
    }
}
int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(capture_and_resume);
    nsx_test_finish();
}
