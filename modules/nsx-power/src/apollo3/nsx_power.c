/**
 * @brief Apollo3/Apollo3p power helpers
 */

#include <stdbool.h>

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_power.h"

static bool s_nsx_power_burst_mode_initialized = false;

uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode) {
    am_hal_burst_mode_e burst_mode;
    am_hal_burst_avail_e burst_available;

    if (!s_nsx_power_burst_mode_initialized) {
        am_hal_burst_mode_initialize(&burst_available);
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

int32_t nsx_power_platform_config(const nsx_power_config_t *cfg) {
    uint32_t control = AM_HAL_MCUCTRL_SRAM_PREFETCH_DATA;

#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (nsx_core_check_api(cfg->api, &nsx_power_oldest_supported_version, &nsx_power_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
    if (!nsx_core_initialized()) {
        return NSX_STATUS_INIT_FAILED;
    }
#endif

    am_bsp_low_power_init();
    NSX_TRY(nsx_power_set_performance_mode(cfg->perf_mode), "Set CPU Perf mode failed.");
    am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
    am_hal_cachectrl_enable();
    am_hal_mcuctrl_control(AM_HAL_MCUCTRL_CONTROL_SRAM_PREFETCH, &control);

    if (cfg->need_tempco) {
        nsx_low_power_printf("WARNING TempCo is not wired for the unified Apollo3 power backend.\n");
    }

    return AM_HAL_STATUS_SUCCESS;
}

void nsx_power_platform_deep_sleep(void) {
    am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
}
