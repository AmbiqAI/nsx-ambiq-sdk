#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "nsx_power.h"
#if defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
#include "am_hal_clkmgr.h"
#endif
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
static void prepare_performance(void) {
    setup_clock();
#if defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
    am_hal_clkmgr_syspll_fref_priority_t priority = {
        .high=AM_HAL_SYSPLL_FREFSEL_HFRC192DIV4,
        .mid=AM_HAL_SYSPLL_FREFSEL_EXTREFCLK,
        .low=AM_HAL_SYSPLL_FREFSEL_XTAL48MHz,
    };
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_clkmgr_control(AM_HAL_CLKMGR_SYPLL_FREF_PRIORITY_SET, &priority));
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_clkmgr_clock_config(AM_HAL_CLKMGR_CLK_ID_PLLVCO, 250000000u, NULL));
#endif
    SysTick->CTRL=0;
    SysTick->LOAD=12345u;
}
static void verify_mode(unsigned level) {
    uint32_t hz, hardware;
    if (level==0) {
        hz=96000000u; hardware=AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER;
    } else {
#if defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
        hz=level==1 ? 192000000u : 250000000u;
        hardware=level==1 ? AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE1 : AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE2;
#elif defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
        hz=192000000u; hardware=AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE;
#else
        hz=250000000u; hardware=AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE;
#endif
    }
    TEST_ASSERT_EQUAL_UINT32(1, PWRCTRL->MCUPERFREQ_b.MCUPERFACK);
    TEST_ASSERT_EQUAL_UINT32(hardware, PWRCTRL->MCUPERFREQ_b.MCUPERFSTATUS);
    TEST_ASSERT_EQUAL_UINT32(hz,SystemCoreClock);
    /* Selection must leave caller-owned timer reloads alone. */
    TEST_ASSERT_EQUAL_UINT32(12345u,SysTick->LOAD);
    TEST_ASSERT_EQUAL_UINT32(0,SysTick->CTRL & SysTick_CTRL_ENABLE_Msk);
    measure(10000u);
    tearDown();
}
static void mixed_public_transitions(void) {
    const nsx_perf_mode_e system[]={NSX_PERF_LOW,NSX_PERF_MEDIUM,NSX_PERF_HIGH};
    const nsx_power_perf_mode_t power[]={NSX_POWER_PERF_LOW,NSX_POWER_PERF_HIGH,NSX_POWER_PERF_MAX};
    const unsigned levels[]={1,2,0,2,1,0};
    for (unsigned i=0;i<sizeof(levels)/sizeof(levels[0]);++i) {
        unsigned level=levels[i];
        TEST_ASSERT_EQUAL_UINT32(0, nsx_set_perf_mode(system[level]));
        verify_mode(level);
        TEST_ASSERT_EQUAL_UINT32(0, nsx_power_set_performance_mode(power[level]));
        verify_mode(level);
    }
}
static void invalid_requests(void) {
    uint32_t published=SystemCoreClock, hardware=PWRCTRL->MCUPERFREQ;
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG,nsx_set_perf_mode((nsx_perf_mode_e)99));
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG,nsx_power_set_performance_mode((nsx_power_perf_mode_t)99));
    TEST_ASSERT_EQUAL_UINT32(published,SystemCoreClock);
    TEST_ASSERT_EQUAL_UINT32(hardware,PWRCTRL->MCUPERFREQ);
}
static void restore_low(void) {
    TEST_ASSERT_EQUAL_UINT32(0,nsx_set_perf_mode(NSX_PERF_LOW));
    TEST_ASSERT_EQUAL_UINT32(96000000u,SystemCoreClock);
    SysTick->CTRL=0;
    SysTick->LOAD=0;
}
int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(prepare_performance);
    if (!Unity.TestFailures) RUN_TEST(mixed_public_transitions);
    if (!Unity.TestFailures) RUN_TEST(invalid_requests);
    RUN_TEST(restore_low);
    nsx_test_finish();
}
