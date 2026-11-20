/** @brief Raw buck pulse counters on Apollo4 and later TIMER targets. */

#include "am_mcu_apollo.h"
#include "nsx_power_buck_internal.h"

/* nsx_timer_config covers 0 through NSX_TIMER_TEMPCO (4), including
 * bare-metal ticker/interrupt services, USB and Apollo4 TempCo. */
enum { FIRST_DEDICATED_TIMER = 5 };

_Static_assert(TIMER15_IRQn - TIMER0_IRQn == AM_REG_NUM_TIMERS - 1,
               "TIMER IRQs must be contiguous");

static const am_hal_timer_clock_e rail_clocks[] = {
    [NSX_BUCK_VDDC] = AM_HAL_TIMER_CLOCK_BUCKC,
    [NSX_BUCK_VDDF] = AM_HAL_TIMER_CLOCK_BUCKF,
    [NSX_BUCK_VDDS] = AM_HAL_TIMER_CLOCK_BUCKS,
    [NSX_BUCK_VDDC_LV] = AM_HAL_TIMER_CLOCK_BUCKC_LV,
};

uint32_t nsx_power_buck_rails(void) {
    return NSX_BUCK_RAIL(NSX_BUCK_VDDC) | NSX_BUCK_RAIL(NSX_BUCK_VDDF) |
           NSX_BUCK_RAIL(NSX_BUCK_VDDS) | NSX_BUCK_RAIL(NSX_BUCK_VDDC_LV);
}

nsx_buck_status_t nsx_buck_hw_check(nsx_buck_rail_t rail, uint8_t timer) {
    if ((unsigned)rail >= sizeof(rail_clocks) / sizeof(rail_clocks[0])) {
        return NSX_BUCK_UNSUPPORTED;
    }
    if (timer >= AM_REG_NUM_TIMERS) {
        return NSX_BUCK_INVALID;
    }
    if (timer < FIRST_DEDICATED_TIMER) {
        return NSX_BUCK_BUSY;
    }
#ifdef AM_HAL_INTERNAL_TIMER_NUM_A
    if (timer == AM_HAL_INTERNAL_TIMER_NUM_A || timer == AM_HAL_INTERNAL_TIMER_NUM_B) {
        return NSX_BUCK_BUSY;
    }
#endif
#ifdef AM_HAL_WRITE_WAIT_TIMER
    if (timer == AM_HAL_WRITE_WAIT_TIMER) {
        return NSX_BUCK_BUSY;
    }
#endif
    /* A stopped synchronization group must keep its global gate unchanged.
     * The HAL's start would otherwise open the gate and stop would leave it open. */
    uint32_t gates = TIMER->GLOBEN;
    if (!(gates & (UINT32_C(1) << timer))) {
        return NSX_BUCK_BUSY;
    }
    /* ADCs can take control of their timer while the local enable is clear. */
    if ((timer == 6 && (gates & TIMER_GLOBEN_AUDADCEN_Msk)) ||
        (timer == 7 && (gates & TIMER_GLOBEN_ADCEN_Msk))) {
        return NSX_BUCK_BUSY;
    }
    IRQn_Type irq = (IRQn_Type)(TIMER0_IRQn + timer);
    const uint32_t compare_irqs = UINT32_C(3) << (2U * timer);
    if ((TIMERn(timer)->CTRL0 & (TIMER_CTRL0_TMR0EN_Msk | TIMER_CTRL0_TMR0TMODE_Msk)) ||
        ((TIMER->INTEN | TIMER->INTSTAT) & compare_irqs) ||
        NVIC_GetEnableIRQ(irq) || NVIC_GetPendingIRQ(irq)) {
        return NSX_BUCK_BUSY;
    }
    return NSX_BUCK_OK;
}

nsx_buck_status_t nsx_buck_hw_start(nsx_buck_rail_t rail, uint8_t timer, bool *touched) {
    am_hal_timer_config_t config;
    *touched = false;
    if (am_hal_timer_default_config_set(&config) != AM_HAL_STATUS_SUCCESS) {
        return NSX_BUCK_HAL_ERROR;
    }
    config.eFunction = AM_HAL_TIMER_FN_UPCOUNT;
    config.eInputClock = rail_clocks[rail];
    /* The pinned HAL performs all configuration checks before its first write. */
    if (am_hal_timer_config(timer, &config) != AM_HAL_STATUS_SUCCESS) {
        return NSX_BUCK_HAL_ERROR;
    }
    *touched = true;
    return am_hal_timer_start(timer) == AM_HAL_STATUS_SUCCESS ? NSX_BUCK_OK : NSX_BUCK_HAL_ERROR;
}

nsx_buck_status_t nsx_buck_hw_stop(uint8_t timer) {
    if (am_hal_timer_stop(timer) != AM_HAL_STATUS_SUCCESS) {
        return NSX_BUCK_HAL_ERROR;
    }
    /* No flags were pending at acquisition. Remove any compare events this
     * session latched, leaving all other channels and the NVIC untouched. */
    const uint32_t compare_irqs = UINT32_C(3) << (2U * timer);
    return am_hal_timer_interrupt_clear(compare_irqs) == AM_HAL_STATUS_SUCCESS
        ? NSX_BUCK_OK : NSX_BUCK_HAL_ERROR;
}

uint32_t nsx_buck_hw_count(uint8_t timer) {
    return am_hal_timer_read(timer);
}

uint32_t nsx_buck_hw_time(uint32_t *tick) {
    uint32_t config = STIMER->STCFG;
    if (config & (STIMER_STCFG_FREEZE_Msk | STIMER_STCFG_CLEAR_Msk)) {
        return 0;
    }

    uint32_t hz;
    switch (config & STIMER_STCFG_CLKSEL_Msk) {
    case AM_HAL_STIMER_HFRC_6MHZ:   hz = 6000000; break;
    case AM_HAL_STIMER_HFRC_375KHZ: hz = 375000; break;
    case AM_HAL_STIMER_XTAL_32KHZ:  hz = 32768; break;
    case AM_HAL_STIMER_XTAL_16KHZ:  hz = 16384; break;
    case AM_HAL_STIMER_XTAL_1KHZ:   hz = 1024; break;
    default: return 0;
    }
    *tick = am_hal_stimer_counter_get();
    return hz;
}

nsx_buck_modes_t nsx_buck_hw_modes(void) {
    nsx_buck_modes_t modes = {NSX_BUCK_SUPPLY_UNKNOWN, NSX_BUCK_SUPPLY_UNKNOWN, NSX_BUCK_CPU_UNKNOWN};
    switch (PWRCTRL->VRSTATUS_b.SIMOBUCKST) {
    case PWRCTRL_VRSTATUS_SIMOBUCKST_OFF: modes.main_supply = NSX_BUCK_SUPPLY_OFF; break;
    case PWRCTRL_VRSTATUS_SIMOBUCKST_LP:  modes.main_supply = NSX_BUCK_SUPPLY_LOW_POWER; break;
    case PWRCTRL_VRSTATUS_SIMOBUCKST_ACT: modes.main_supply = NSX_BUCK_SUPPLY_ACTIVE; break;
    }
    /* Read ACK and status together; the status is valid only while acknowledged. */
    uint32_t perf = PWRCTRL->MCUPERFREQ;
    if (perf & PWRCTRL_MCUPERFREQ_MCUPERFACK_Msk) {
        switch (_FLD2VAL(PWRCTRL_MCUPERFREQ_MCUPERFSTATUS, perf)) {
        case PWRCTRL_MCUPERFREQ_MCUPERFSTATUS_LP: modes.cpu_mode = NSX_BUCK_CPU_LP; break;
        case PWRCTRL_MCUPERFREQ_MCUPERFSTATUS_HP: modes.cpu_mode = NSX_BUCK_CPU_HP; break;
#if defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
        case PWRCTRL_MCUPERFREQ_MCUPERFSTATUS_ULP: modes.cpu_mode = NSX_BUCK_CPU_ULP; break;
#endif
        }
    }
    return modes;
}
