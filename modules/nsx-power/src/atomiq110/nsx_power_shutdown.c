/* AT110 peripheral shutdown for power measurements.
 * Copyright (c) 2026, Ambiq Micro, Inc.
 */
#include "am_mcu_apollo.h"
#include "nsx_debug_internal.h"
#include "nsx_power_internal.h"

uint32_t nsx_power_shutdown_peripherals(void) {
    NSX_POWER_RETURN_IF_ERROR(nsx_debug_before_hw_init());
    NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_DIS_PERIPHS_ALL, NULL));
    NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_XTAL_PWDN_DEEPSLEEP, NULL));
    VCOMP->PWDKEY = VCOMP_PWDKEY_PWDKEY_Key;

    for (uint32_t timer = 0; timer < AM_REG_NUM_TIMERS; ++timer) {
        if (timer == AM_HAL_INTERNAL_TIMER_NUM_A || timer == AM_HAL_INTERNAL_TIMER_NUM_B) continue;
        NSX_POWER_RETURN_IF_ERROR(am_hal_timer_stop(timer));
    }
    return NSX_STATUS_SUCCESS;
}
