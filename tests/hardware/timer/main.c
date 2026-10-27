#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "nsx_timer.h"
#include "nsx_interrupt.h"
#include "unity.h"
#include "result_reporter.h"

#define SAMPLES 8u
static volatile uint32_t cycles[SAMPLES], count;
static nsx_timer_config_t cfg;
static bool attempted;
void setUp(void) {}
void tearDown(void) {
    if (!attempted) return;
    __disable_irq();
#if defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
    uint32_t status=nsx_timer_deinit(&cfg);
#else
    uint32_t status=am_hal_timer_stop(cfg.timer);
    status |= am_hal_timer_interrupt_disable(AM_HAL_TIMER_MASK(cfg.timer, AM_HAL_TIMER_COMPARE1));
    status |= am_hal_timer_interrupt_clear(AM_HAL_TIMER_MASK(cfg.timer, AM_HAL_TIMER_COMPARE1));
    status |= nsx_irq_unregister(TIMER1_IRQn);
    status |= nsx_irq_clear_pending(TIMER1_IRQn);
#endif
    attempted=false;
    __enable_irq();
    TEST_ASSERT_EQUAL_UINT32(0, status);
}
static void callback(nsx_timer_config_t *timer) {
    (void)timer;
    if (count<SAMPLES) { cycles[count]=DWT->CYCCNT; ++count; }
}
static void setup_clock(void) {
    nsx_system_config_t system={.enable_cache=true,.perf_mode=NSX_PERF_LOW,
                                .debug={.transport=NSX_DEBUG_ITM}};
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&system));
    TEST_ASSERT_EQUAL_UINT32(96000000u, SystemCoreClock);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t before=DWT->CYCCNT;
    for (volatile unsigned i=0;i<1000;++i) {}
    TEST_ASSERT_NOT_EQUAL(before, DWT->CYCCNT);
    __enable_irq();
}
static void measure(uint32_t period) {
    TEST_ASSERT_BITS_HIGH(DWT_CTRL_CYCCNTENA_Msk, DWT->CTRL);
    count=0;
    cfg=(nsx_timer_config_t){.api=&nsx_timer_current_version,.timer=NSX_TIMER_INTERRUPT,
        .enableInterrupt=true,.periodInMicroseconds=period,.callback=callback};
    attempted=true;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_timer_init(&cfg));
    uint32_t start=DWT->CYCCNT;
    uint32_t expected=(SystemCoreClock/1000000u)*period;
    /* DWT reference plus host transport deadline bound a missing interrupt. */
    while (count<SAMPLES && (uint32_t)(DWT->CYCCNT-start)<expected*12u) {}
    TEST_ASSERT_EQUAL_UINT32(SAMPLES,count);
    UnityPrint("period_us="); UnityPrintNumberUnsigned(period);
    UnityPrint(" cpu_hz="); UnityPrintNumberUnsigned(SystemCoreClock);
    UnityPrint(" intervals_cycles=");
    for (unsigned i=1;i<SAMPLES;++i) {
        uint32_t delta=cycles[i]-cycles[i-1];
        UnityPrintNumberUnsigned(delta); UnityPrint(" ");
    }
    UnityPrint("\n");
    for (unsigned i=1;i<SAMPLES;++i) {
        uint32_t delta=cycles[i]-cycles[i-1];
        TEST_ASSERT_UINT32_WITHIN(expected/5u,expected,delta);
    }
}
static void period_1ms(void) { measure(1000); }
static void period_10ms(void) { measure(10000); }
static void period_50ms(void) { measure(50000); }
int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(setup_clock);
    RUN_TEST(period_1ms);
    RUN_TEST(period_10ms);
    RUN_TEST(period_50ms);
    nsx_test_finish();
}
