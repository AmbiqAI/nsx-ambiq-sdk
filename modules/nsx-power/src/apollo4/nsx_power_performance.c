/** @brief Family performance-mode adapter. */

#include "am_mcu_apollo.h"
#include "nsx_power.h"

uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode) {
    if ((mode == NSX_POWER_PERF_HIGH) || (mode == NSX_POWER_PERF_MAX)) {
        return am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE);
    }
    if (mode == NSX_POWER_PERF_LOW) {
        return am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER);
    }
    return NSX_STATUS_INVALID_CONFIG;
}
