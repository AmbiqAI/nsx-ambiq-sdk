/** @brief Family performance-mode adapter. */

#include "am_mcu_apollo.h"
#include "nsx_power.h"

static bool s_nsx_power_burst_mode_initialized = false;

uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode) {
    am_hal_burst_mode_e burst_mode;
    am_hal_burst_avail_e burst_available = AM_HAL_BURST_NOTAVAIL;

    if (mode != NSX_POWER_PERF_LOW && mode != NSX_POWER_PERF_HIGH && mode != NSX_POWER_PERF_MAX) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    if (!s_nsx_power_burst_mode_initialized) {
        uint32_t status = am_hal_burst_mode_initialize(&burst_available);
        if (status != AM_HAL_STATUS_SUCCESS) {
            return status;
        }
        if (burst_available != AM_HAL_BURST_AVAIL) {
            return NSX_STATUS_FAILURE;
        }
        s_nsx_power_burst_mode_initialized = true;
    }

    if ((mode == NSX_POWER_PERF_HIGH) || (mode == NSX_POWER_PERF_MAX)) {
        return am_hal_burst_mode_enable(&burst_mode);
    }
    if (mode == NSX_POWER_PERF_LOW) {
        return am_hal_burst_mode_disable(&burst_mode);
    }
    return NSX_STATUS_INVALID_CONFIG;
}
