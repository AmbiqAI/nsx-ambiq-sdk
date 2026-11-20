#include "am_mcu_apollo.h"
#include <assert.h>

static struct { uint32_t CTRL0; } channels[AM_REG_NUM_TIMERS];
static struct { uint32_t GLOBEN, INTEN, INTSTAT; } timers;
static struct { uint32_t STCFG; } stimer;
static struct {
    struct { uint32_t SIMOBUCKST; } VRSTATUS_b;
    uint32_t MCUPERFREQ;
} power;
static IRQn_Type irq_enabled, irq_pending;
#undef TIMERn
#undef TIMER
#undef STIMER
#undef PWRCTRL
#define TIMERn(n) (&channels[n])
#define TIMER (&timers)
#define STIMER (&stimer)
#define PWRCTRL (&power)
#define NVIC_GetEnableIRQ(irq) ((irq) == irq_enabled)
#define NVIC_GetPendingIRQ(irq) ((irq) == irq_pending)

#include "nsx_power_buck_timer.c"

enum { CHANNEL = 8 };
static am_hal_timer_config_t programmed;
static uint32_t start_result, stop_result;
uint32_t am_hal_timer_default_config_set(am_hal_timer_config_t *config) {
    *config = (am_hal_timer_config_t){0};
    return AM_HAL_STATUS_SUCCESS;
}
uint32_t am_hal_timer_config(uint32_t timer, am_hal_timer_config_t *config) {
    assert(timer == CHANNEL);
    programmed = *config;
    return AM_HAL_STATUS_SUCCESS;
}
uint32_t am_hal_timer_enable(uint32_t timer) { assert(timer == CHANNEL); return start_result; }
uint32_t am_hal_timer_disable(uint32_t timer) { assert(timer == CHANNEL); return stop_result; }
uint32_t am_hal_timer_interrupt_clear(uint32_t mask) {
    assert(mask == (3u << (2 * CHANNEL)));
    timers.INTSTAT &= ~mask;
    return AM_HAL_STATUS_SUCCESS;
}
uint32_t am_hal_timer_read(uint32_t timer) { assert(timer == CHANNEL); return 0x13579bdf; }
uint32_t am_hal_stimer_counter_get(void) { return 0xabcdef12; }

static void check_ownership(void) {
    timers.GLOBEN = AM_HAL_TIMER_GLOBEN_DEFAULT;
    assert(nsx_buck_hw_check(NSX_BUCK_BLE, CHANNEL) == NSX_BUCK_UNSUPPORTED);
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, AM_REG_NUM_TIMERS) == NSX_BUCK_INVALID);
    for (unsigned i = 0; i <= 4; i++) assert(nsx_buck_hw_check(NSX_BUCK_VDDC, i) == NSX_BUCK_BUSY);
#ifdef AM_HAL_WRITE_WAIT_TIMER
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, AM_HAL_WRITE_WAIT_TIMER) == NSX_BUCK_BUSY);
#endif
#ifdef AM_HAL_INTERNAL_TIMER_NUM_A
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, AM_HAL_INTERNAL_TIMER_NUM_A) == NSX_BUCK_BUSY);
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, AM_HAL_INTERNAL_TIMER_NUM_B) == NSX_BUCK_BUSY);
#endif
    timers.GLOBEN &= ~(1u << CHANNEL);
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, CHANNEL) == NSX_BUCK_BUSY);
    timers.GLOBEN = AM_HAL_TIMER_GLOBEN_DEFAULT;
    timers.GLOBEN |= TIMER_GLOBEN_AUDADCEN_Msk | TIMER_GLOBEN_ADCEN_Msk;
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, 6) == NSX_BUCK_BUSY);
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, 7) == NSX_BUCK_BUSY);
    channels[CHANNEL].CTRL0 = TIMER_CTRL0_TMR0EN_Msk;
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, CHANNEL) == NSX_BUCK_BUSY);
    channels[CHANNEL].CTRL0 = TIMER_CTRL0_TMR0TMODE_Msk;
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, CHANNEL) == NSX_BUCK_BUSY);
    channels[CHANNEL].CTRL0 = 0;
    timers.INTEN = 2u << (2 * CHANNEL);
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, CHANNEL) == NSX_BUCK_BUSY);
    timers.INTEN = 0;
    timers.INTSTAT = 1u << (2 * CHANNEL);
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, CHANNEL) == NSX_BUCK_BUSY);
    timers.INTSTAT = 0;
    irq_enabled = TIMER8_IRQn;
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, CHANNEL) == NSX_BUCK_BUSY);
    irq_enabled = TIMER7_IRQn;
    irq_pending = TIMER8_IRQn;
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, CHANNEL) == NSX_BUCK_BUSY);
    irq_pending = TIMER9_IRQn;
    assert(nsx_buck_hw_check(NSX_BUCK_VDDC, CHANNEL) == NSX_BUCK_OK);
}

static void check_counters(void) {
    assert(nsx_power_buck_rails() == 15);
    const am_hal_timer_clock_e expected[] = { AM_HAL_TIMER_CLOCK_BUCKC, AM_HAL_TIMER_CLOCK_BUCKF,
        AM_HAL_TIMER_CLOCK_BUCKS, AM_HAL_TIMER_CLOCK_BUCKC_LV };
    for (unsigned r = 0; r < 4; ++r) {
        bool touched = false;
        assert(nsx_buck_hw_start((nsx_buck_rail_t)r, CHANNEL, &touched) == NSX_BUCK_OK && touched);
        assert(programmed.eFunction == AM_HAL_TIMER_FN_UPCOUNT && programmed.eInputClock == expected[r]);
        assert(nsx_buck_hw_count(CHANNEL) == 0x13579bdf);
        timers.INTSTAT = (3u << (2 * CHANNEL)) | 1u;
        assert(nsx_buck_hw_stop(CHANNEL) == NSX_BUCK_OK);
        assert(timers.INTSTAT == 1u);
    }
    bool touched = false;
    start_result = AM_HAL_STATUS_FAIL;
    assert(nsx_buck_hw_start(NSX_BUCK_VDDC, CHANNEL, &touched) == NSX_BUCK_HAL_ERROR && touched);
    stop_result = AM_HAL_STATUS_FAIL;
    timers.INTSTAT = 3u << (2 * CHANNEL);
    assert(nsx_buck_hw_stop(CHANNEL) == NSX_BUCK_HAL_ERROR);
    assert(timers.INTSTAT == (3u << (2 * CHANNEL)));
}

static void check_time_and_modes(void) {
    uint32_t ticks;
    stimer.STCFG = AM_HAL_STIMER_XTAL_32KHZ;
    assert(nsx_buck_hw_time(&ticks) == 32768 && ticks == 0xabcdef12);
    stimer.STCFG = AM_HAL_STIMER_XTAL_16KHZ;
    assert(nsx_buck_hw_time(&ticks) == 16384);
    stimer.STCFG |= STIMER_STCFG_FREEZE_Msk;
    assert(nsx_buck_hw_time(&ticks) == 0);
    stimer.STCFG = AM_HAL_STIMER_HFRC_CTIMER0A;
    assert(nsx_buck_hw_time(&ticks) == 0);
    power.MCUPERFREQ = _VAL2FLD(PWRCTRL_MCUPERFREQ_MCUPERFSTATUS, PWRCTRL_MCUPERFREQ_MCUPERFSTATUS_LP);
    assert(nsx_buck_hw_modes().cpu_mode == NSX_BUCK_CPU_UNKNOWN);
    power.MCUPERFREQ |= PWRCTRL_MCUPERFREQ_MCUPERFACK_Msk;
    assert(nsx_buck_hw_modes().cpu_mode == NSX_BUCK_CPU_LP);
    power.MCUPERFREQ = PWRCTRL_MCUPERFREQ_MCUPERFACK_Msk |
                       _VAL2FLD(PWRCTRL_MCUPERFREQ_MCUPERFSTATUS, PWRCTRL_MCUPERFREQ_MCUPERFSTATUS_HP);
    assert(nsx_buck_hw_modes().cpu_mode == NSX_BUCK_CPU_HP);
    const nsx_buck_supply_t expected[] = {NSX_BUCK_SUPPLY_OFF, NSX_BUCK_SUPPLY_UNKNOWN,
        NSX_BUCK_SUPPLY_LOW_POWER, NSX_BUCK_SUPPLY_ACTIVE};
    for (unsigned i = 0; i < 4; ++i) {
        power.VRSTATUS_b.SIMOBUCKST = i;
        assert(nsx_buck_hw_modes().main_supply == expected[i]);
    }
}

int main(void) {
    check_ownership();
    check_counters();
    check_time_and_modes();
}
