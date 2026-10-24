#include <stdio.h>
#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "nsx_power.h"
#include "unity.h"
#include "result_reporter.h"
#include "measurement.h"

/* Experimental envelope: see docs/clock-timebase-qualification.md. */
#define REFERENCE_HZ 32768U
#define WINDOW_TICKS 4096U
#define MAX_APERTURE_CYCLES 2048U
#define POLL_BUDGET 2000000U
#define TICK_HZ 1000U
static volatile uint32_t ticks;
static bool initialized;
static uint32_t saved_primask, saved_basepri;

void SysTick_Handler(void) { ++ticks; }
void setUp(void) {}
void tearDown(void) {
    SysTick->CTRL = 0;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    uint32_t status = initialized ? nsx_power_set_performance_mode(NSX_POWER_PERF_LOW) : 0;
    __set_BASEPRI(saved_basepri);
    __set_PRIMASK(saved_primask);
    if (initialized) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, status, "LOW cleanup failed");
        TEST_ASSERT_EQUAL_UINT32(96000000, SystemCoreClock);
        TEST_ASSERT_EQUAL_UINT32(1, PWRCTRL->MCUPERFREQ_b.MCUPERFSTATUS);
    }
}

static void log_sample(unsigned row, uint32_t cycles, uint32_t refs, uint32_t irq) {
    char line[80];
    snprintf(line, sizeof line, "r%u c%lu x%lu t%lu\n", row, (unsigned long)cycles,
             (unsigned long)refs, (unsigned long)irq);
    for (char *p=line; *p; ++p) nsx_test_putchar(*p);
}

static bool wait_reference(uint32_t count) {
    uint32_t start=am_hal_stimer_counter_get();
    for (unsigned i=0; i<POLL_BUDGET; ++i) {
        if ((uint32_t)(am_hal_stimer_counter_get()-start)>=count) return true;
    }
    return false;
}

static void reference_ready(void) {
    am_bsp_low_power_init();
    initialized=true;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_power_set_performance_mode(NSX_POWER_PERF_LOW));
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_mcuctrl_control(AM_HAL_MCUCTRL_CONTROL_EXTCLK32K_ENABLE, NULL));
    am_hal_mcuctrl_ext32k_status_e status;
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_mcuctrl_extclk32k_status_get(&status));
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_MCUCTRL_EXT32K_STATUS_XTAL, status);
    am_hal_stimer_config(STIMER_STCFG_CLEAR_Msk | STIMER_STCFG_FREEZE_Msk);
    am_hal_stimer_counter_clear();
    am_hal_stimer_config(AM_HAL_STIMER_XTAL_32KHZ);
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_STIMER_XTAL_32KHZ, STIMER->STCFG);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    TEST_ASSERT_BITS_HIGH(DWT_CTRL_CYCCNTENA_Msk, DWT->CTRL);
    uint32_t before=DWT->CYCCNT;
    /* Warm, awake diagnostic pre-roll, not a cold-start settling guarantee. */
    TEST_ASSERT_TRUE_MESSAGE(wait_reference(REFERENCE_HZ/2), "Reference stalled");
    TEST_ASSERT_NOT_EQUAL(before, DWT->CYCCNT);
#if defined(AM_PART_APOLLO330P)
    am_hal_clkmgr_syspll_fref_priority_t priority = {
        .high=AM_HAL_SYSPLL_FREFSEL_HFRC192DIV4,
        .mid=AM_HAL_SYSPLL_FREFSEL_EXTREFCLK,
        .low=AM_HAL_SYSPLL_FREFSEL_XTAL48MHz,
    };
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_clkmgr_control(AM_HAL_CLKMGR_SYPLL_FREF_PRIORITY_SET, &priority));
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_clkmgr_clock_config(AM_HAL_CLKMGR_CLK_ID_PLLVCO, 250000000, NULL));
    am_hal_clkmgr_clkcfg_t cfg;
    uint32_t hz;
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_clkmgr_clock_config_get(AM_HAL_CLKMGR_CLK_ID_PLLVCO, &hz, &cfg));
    TEST_ASSERT_EQUAL_UINT32(250000000,hz);
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_SYSPLL_FREFSEL_HFRC192DIV4,cfg.syspll.eFref);
#else
    TEST_ASSERT_EQUAL_UINT32(0, CLKGEN->HF2ADJ0_b.HF2ADJEN);
#endif
}

static uint32_t nominal(nsx_power_perf_mode_t mode) {
    if (mode==NSX_POWER_PERF_LOW || mode==NSX_POWER_PERF_NOT_SET) return 96000000;
#if defined(AM_PART_APOLLO330P)
    if (mode==NSX_POWER_PERF_HIGH) return 192000000;
#endif
    return 250000000;
}

static void select_mode(nsx_power_perf_mode_t mode) {
    TEST_ASSERT_EQUAL_UINT32(0, nsx_power_set_performance_mode(mode));
    uint32_t hardware=mode==NSX_POWER_PERF_LOW || mode==NSX_POWER_PERF_NOT_SET ? 1 : 2;
#if defined(AM_PART_APOLLO330P)
    if (mode==NSX_POWER_PERF_MAX) hardware=3;
#endif
    uint32_t start=am_hal_stimer_counter_get();
    unsigned budget=POLL_BUDGET;
    while ((!PWRCTRL->MCUPERFREQ_b.MCUPERFACK || PWRCTRL->MCUPERFREQ_b.MCUPERFSTATUS!=hardware) && --budget) {
        TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(REFERENCE_HZ/100, (uint32_t)(am_hal_stimer_counter_get()-start), "Mode readiness deadline");
    }
    TEST_ASSERT_NOT_EQUAL(0, budget);
    TEST_ASSERT_EQUAL_UINT32(nominal(mode), SystemCoreClock);
#if defined(AM_PART_APOLLO330P)
    if (hardware==3) {
        TEST_ASSERT_EQUAL_UINT32(1, MCUCTRL->PLLSTAT_b.LOCK);
        TEST_ASSERT_EQUAL_UINT32(AM_HAL_SYSPLL_FREFSEL_HFRC192DIV4, MCUCTRL->PLLCTL0_b.FREFSEL);
        uint64_t feedback=((uint64_t)MCUCTRL->PLLDIV1_b.FBDIV<<24) + MCUCTRL->PLLDIV0;
        TEST_ASSERT_TRUE(feedback*48000000 == ((uint64_t)250000000*MCUCTRL->PLLDIV1_b.REFDIV<<24));
    }
#else
    if (hardware==2) TEST_ASSERT_EQUAL_UINT32(1, CLKGEN->CLOCKENSTAT_b.HFRC2READY);
    TEST_ASSERT_EQUAL_UINT32(0, CLKGEN->HF2ADJ0_b.HF2ADJEN);
#endif
}

typedef struct { uint32_t before, ref, after, irq; } Snapshot;
static Snapshot snapshot(void) {
    uint32_t mask=__get_PRIMASK();
    __disable_irq();
    Snapshot s;
    s.irq=ticks;
    s.before=DWT->CYCCNT;
    s.ref=am_hal_stimer_counter_get();
    s.after=DWT->CYCCNT;
    __set_PRIMASK(mask);
    return s;
}

static void capture(unsigned row, uint32_t hz) {
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_STIMER_XTAL_32KHZ, STIMER->STCFG);
    Snapshot a=snapshot(), b=a;
    unsigned budget=POLL_BUDGET;
    while ((uint32_t)(b.ref-a.ref)<WINDOW_TICKS && --budget) {
        b=snapshot();
        TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(300000000, b.after-a.after, "Cycle acquisition bound");
    }
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_STIMER_XTAL_32KHZ, STIMER->STCFG);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, budget, "Capture stalled");
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(MAX_APERTURE_CYCLES, a.after-a.before);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(MAX_APERTURE_CYCLES, b.after-b.before);
    nsx_clock_sample_t sample;
    TEST_ASSERT_TRUE(nsx_clock_sample(a.after,b.after,a.ref,b.ref,WINDOW_TICKS+8,300000000,32766,&sample));
    uint32_t irq=b.irq-a.irq;
    log_sample(row,sample.cpu_cycles,sample.reference_ticks,irq);
    uint32_t reference=REFERENCE_HZ;
#ifdef CLOCK_WRONG_SCALING
    reference*=2;
#endif
    /* 5% source + reference/aperture budget; experimental until EVB evidence. */
    TEST_ASSERT_TRUE_MESSAGE(nsx_clock_rate_within(sample,reference,
        (uint64_t)hz*948/1000,(uint64_t)hz*1052/1000), "Physical clock envelope");
    uint32_t reload=hz/TICK_HZ;
    uint64_t counted=(uint64_t)irq*reload;
    uint64_t error=counted>sample.cpu_cycles ? counted-sample.cpu_cycles : sample.cpu_cycles-counted;
    TEST_ASSERT_TRUE_MESSAGE(error <= (uint64_t)reload*2+MAX_APERTURE_CYCLES*2, "SysTick count/reload disagrees with measured cycles");
}

static void clock_transitions(void) {
    TEST_ASSERT_TRUE(initialized);
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_STIMER_XTAL_32KHZ, STIMER->STCFG);
    const nsx_power_perf_mode_t L=NSX_POWER_PERF_LOW,H=NSX_POWER_PERF_HIGH,M=NSX_POWER_PERF_MAX,N=NSX_POWER_PERF_NOT_SET;
    const nsx_power_perf_mode_t rows[][2]={
        {L,L},{L,H},{H,H},{H,M},{L,M},{M,M},{M,H},{H,L},{M,L},{L,N},{H,N},{M,N}
    };
    const uintptr_t *vectors=(const uintptr_t *)SCB->VTOR;
    TEST_ASSERT_EQUAL_HEX32((uintptr_t)SysTick_Handler,vectors[16+SysTick_IRQn]);
    __set_BASEPRI(0);
    __enable_irq();
    for (unsigned repeat=0; repeat<2; ++repeat) {
        for (unsigned row=0; row<sizeof rows/sizeof rows[0]; ++row) {
            select_mode(rows[row][0]);
            TEST_ASSERT_EQUAL_UINT32(0, SysTick_Config(nominal(rows[row][0])/TICK_HZ));
            select_mode(rows[row][1]);
#ifdef CLOCK_STALE_PUBLICATION
            if (nominal(rows[row][1])!=96000000) SystemCoreClock=96000000;
#endif
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(nominal(rows[row][1]), SystemCoreClock, "Stale nominal clock");
#ifndef CLOCK_NO_RELOAD
            SysTick->CTRL=0;
            SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk;
            TEST_ASSERT_EQUAL_UINT32(0, SysTick_Config(SystemCoreClock/TICK_HZ));
#endif
            for (unsigned sample=0; sample<3; ++sample) capture(repeat*12+row,nominal(rows[row][1]));
            SysTick->CTRL=0;
            SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk;
            select_mode(L);
        }
    }
}

static void restore_low(void) { TEST_ASSERT_TRUE(initialized); select_mode(NSX_POWER_PERF_LOW); }
int main(void) {
    saved_primask=__get_PRIMASK();
    saved_basepri=__get_BASEPRI();
    nsx_test_begin("clock");
    RUN_TEST(reference_ready);
    RUN_TEST(clock_transitions);
    RUN_TEST(restore_low);
    nsx_test_finish();
}
