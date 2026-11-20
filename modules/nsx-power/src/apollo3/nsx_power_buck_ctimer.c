/* Apollo3 and Apollo3 Plus use a linked A/B CTIMER pair for each rail. */
#include "nsx_power_buck_internal.h"

#include "am_mcu_apollo.h"

uint32_t nsx_power_buck_rails(void) {
    return NSX_BUCK_RAIL(NSX_BUCK_CPU) | NSX_BUCK_RAIL(NSX_BUCK_MEMORY) |
           NSX_BUCK_RAIL(NSX_BUCK_BLE);
}

static uint32_t pair_interrupt_mask(uint8_t timer) {
    const uint32_t pair0 = AM_HAL_CTIMER_INT_TIMERA0C0 | AM_HAL_CTIMER_INT_TIMERA0C1 |
                           AM_HAL_CTIMER_INT_TIMERB0C0 | AM_HAL_CTIMER_INT_TIMERB0C1;
    return pair0 << (2U * timer);
}

nsx_buck_status_t nsx_buck_hw_check(nsx_buck_rail_t rail, uint8_t timer) {
    if (rail != NSX_BUCK_CPU && rail != NSX_BUCK_MEMORY && rail != NSX_BUCK_BLE) {
        return NSX_BUCK_UNSUPPORTED;
    }
    if (timer >= AM_HAL_CTIMER_TIMERS_NUM) {
        return NSX_BUCK_INVALID;
    }
    if (timer == AM_HAL_ENTROPY_CTIMER) {
        return NSX_BUCK_BUSY;
    }
    /* The caller dedicates both halves. Also reject an already configured
     * pair, auxiliary trigger configuration, and enabled or pending IRQs. */
    if (CTIMERn(timer)->CTRL0 != 0 || CTIMERn(timer)->AUX0 != 0 ||
        ((CTIMER->INTEN | CTIMER->INTSTAT) & pair_interrupt_mask(timer)) != 0) {
        return NSX_BUCK_BUSY;
    }
    return NSX_BUCK_OK;
}

nsx_buck_status_t nsx_buck_hw_start(nsx_buck_rail_t rail, uint8_t timer, bool *touched) {
    uint32_t selector;
    switch (rail) {
        case NSX_BUCK_CPU:    selector = CTIMER_CTRL0_TMRA0CLK_BUCKA; break;
        case NSX_BUCK_MEMORY: selector = CTIMER_CTRL0_TMRA0CLK_BUCKB; break;
        case NSX_BUCK_BLE:    selector = CTIMER_CTRL0_TMRA0CLK_BUCKBLE; break;
        default: return NSX_BUCK_UNSUPPORTED;
    }
    *touched = true;
    /* BOTH links B as the high half of A. The clock selector is in A. */
    am_hal_ctimer_config_single(timer, AM_HAL_CTIMER_BOTH, AM_HAL_CTIMER_FN_CONTINUOUS |
                                _VAL2FLD(CTIMER_CTRL0_TMRA0CLK, selector));
    am_hal_ctimer_start(timer, AM_HAL_CTIMER_BOTH);
    return NSX_BUCK_OK;
}

nsx_buck_status_t nsx_buck_hw_stop(uint8_t timer) {
    am_hal_ctimer_stop(timer, AM_HAL_CTIMER_BOTH);
    am_hal_ctimer_clear(timer, AM_HAL_CTIMER_BOTH);
    /* Preflight accepted only an unconfigured pair. Unlink it for reuse. */
    CTIMERn(timer)->CTRL0 = 0;
    return NSX_BUCK_OK;
}

uint32_t nsx_buck_hw_count(uint8_t timer) {
    return am_hal_ctimer_read(timer, AM_HAL_CTIMER_BOTH);
}

uint32_t nsx_buck_hw_time(uint32_t *tick) {
    const uint32_t config = CTIMER->STCFG;
    if (config & (CTIMER_STCFG_CLEAR_Msk | CTIMER_STCFG_FREEZE_Msk)) {
        return 0;
    }

    uint32_t hz;
    switch (_FLD2VAL(CTIMER_STCFG_CLKSEL, config)) {
        case CTIMER_STCFG_CLKSEL_HFRC_DIV16:  hz = 3000000; break;
        case CTIMER_STCFG_CLKSEL_HFRC_DIV256: hz = 187500; break;
        case CTIMER_STCFG_CLKSEL_XTAL_DIV1:   hz = 32768; break;
        case CTIMER_STCFG_CLKSEL_XTAL_DIV2:   hz = 16384; break;
        case CTIMER_STCFG_CLKSEL_XTAL_DIV32:  hz = 1024; break;
        /* LFRC is uncalibrated and has the same nominal frequency as XTAL/32.
         * Only accept direct HFRC/XTAL taps with distinct endpoint rates. */
        default: return 0;
    }
    *tick = am_hal_stimer_counter_get();
    return hz;
}

nsx_buck_modes_t nsx_buck_hw_modes(void) {
    const uint32_t status = PWRCTRL->SUPPLYSTATUS;
    return (nsx_buck_modes_t){
        .main_supply = (status & PWRCTRL_SUPPLYSTATUS_SIMOBUCKON_Msk) ? NSX_BUCK_SUPPLY_ON : NSX_BUCK_SUPPLY_OFF,
        .ble_supply = (status & PWRCTRL_SUPPLYSTATUS_BLEBUCKON_Msk) ? NSX_BUCK_SUPPLY_ON : NSX_BUCK_SUPPLY_OFF,
        .cpu_mode = am_hal_burst_mode_status() == AM_HAL_BURST_MODE ? NSX_BUCK_CPU_BURST : NSX_BUCK_CPU_NORMAL,
    };
}
