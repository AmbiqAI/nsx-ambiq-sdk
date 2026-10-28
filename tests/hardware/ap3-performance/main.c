#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "nsx_power.h"
#include "unity.h"
#include "result_reporter.h"

void setUp(void) {}
void tearDown(void) {}

static void measure(uint32_t hz) {
    TEST_ASSERT_EQUAL_UINT32(hz, SystemCoreClock);
    uint32_t start=am_hal_stimer_counter_get(), cycles=DWT->CYCCNT, elapsed;
    unsigned polls=0;
    do {
        elapsed=am_hal_stimer_counter_get()-start;
    } while(elapsed<30000u && ++polls<1000000u);
    uint32_t delta=DWT->CYCCNT-cycles;
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(30000u, elapsed);
    /* CORESEL divides all internal HFRC sources, including STIMER. */
    uint32_t reference_hz=3000000u >> CLKGEN->CCTRL_b.CORESEL;
    uint32_t expected=elapsed*(hz/reference_hz);
    UnityPrint("cpu_hz="); UnityPrintNumberUnsigned(hz);
    UnityPrint(" stimer_ticks="); UnityPrintNumberUnsigned(elapsed);
    UnityPrint(" dwt_cycles="); UnityPrintNumberUnsigned(delta); UnityPrint("\n");
    TEST_ASSERT_UINT32_WITHIN(expected/20u, expected, delta);
}
static void prepare(void) {
    nsx_system_config_t cfg={.enable_cache=true,.perf_mode=NSX_PERF_LOW};
    TEST_ASSERT_EQUAL_UINT32(0,nsx_system_init(&cfg));
    TEST_ASSERT_EQUAL_UINT32(0,am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX,0));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_set_perf_mode(NSX_PERF_LOW));
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    am_hal_stimer_config(AM_HAL_STIMER_HFRC_3MHZ);
    measure(48000000u);
}
static void mixed_transitions(void) {
    for(unsigned i=0;i<4;++i) {
        TEST_ASSERT_EQUAL_UINT32(0,nsx_set_perf_mode(NSX_PERF_MEDIUM)); measure(96000000u);
        TEST_ASSERT_EQUAL_UINT32(0,nsx_power_set_performance_mode(NSX_POWER_PERF_MAX)); measure(96000000u);
        TEST_ASSERT_EQUAL_UINT32(0,nsx_set_perf_mode(NSX_PERF_LOW)); measure(48000000u);
        TEST_ASSERT_EQUAL_UINT32(0,nsx_power_set_performance_mode(NSX_POWER_PERF_HIGH)); measure(96000000u);
        TEST_ASSERT_EQUAL_UINT32(0,nsx_power_set_performance_mode(NSX_POWER_PERF_LOW)); measure(48000000u);
    }
}
static void divided_normal(void) {
    TEST_ASSERT_EQUAL_UINT32(0,nsx_set_perf_mode(NSX_PERF_LOW));
    TEST_ASSERT_EQUAL_UINT32(0,am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_DIV2,0));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_set_perf_mode(NSX_PERF_LOW)); measure(24000000u);
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG,nsx_set_perf_mode(NSX_PERF_HIGH));
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG,nsx_power_set_performance_mode(NSX_POWER_PERF_MAX));
    measure(24000000u);
    TEST_ASSERT_EQUAL_UINT32(0,am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX,0));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_power_set_performance_mode(NSX_POWER_PERF_LOW)); measure(48000000u);
}
static void restore_normal(void) {
    TEST_ASSERT_EQUAL_UINT32(0,nsx_set_perf_mode(NSX_PERF_LOW));
    TEST_ASSERT_EQUAL_UINT32(0,am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX,0));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_set_perf_mode(NSX_PERF_LOW));
}
int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(prepare);
    if(Unity.TestFailures==0) RUN_TEST(mixed_transitions);
    if(Unity.TestFailures==0) RUN_TEST(divided_normal);
    RUN_TEST(restore_normal);
    nsx_test_finish();
}
