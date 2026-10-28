/**
 * @brief Apollo3/Apollo3p power helpers
 */

#include <stdbool.h>

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_power_internal.h"

uint32_t nsx_power_platform_validate_config(const nsx_power_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (cfg->api == NULL || nsx_core_check_api(cfg->api, &nsx_power_oldest_supported_version, &nsx_power_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    if (cfg->perf_mode != NSX_POWER_PERF_LOW && cfg->perf_mode != NSX_POWER_PERF_HIGH &&
        cfg->perf_mode != NSX_POWER_PERF_MAX) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (cfg->need_tempco) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_power_platform_config(const nsx_power_config_t *cfg) {
    uint32_t control = AM_HAL_MCUCTRL_SRAM_PREFETCH_DATA;
    NSX_POWER_RETURN_IF_ERROR(nsx_power_platform_validate_config(cfg));
    am_bsp_low_power_init();
    NSX_POWER_RETURN_IF_ERROR(nsx_power_set_performance_mode(cfg->perf_mode));
    NSX_POWER_RETURN_IF_ERROR(am_hal_cachectrl_config(&am_hal_cachectrl_defaults));
    NSX_POWER_RETURN_IF_ERROR(am_hal_cachectrl_enable());
    NSX_POWER_RETURN_IF_ERROR(am_hal_mcuctrl_control(AM_HAL_MCUCTRL_CONTROL_SRAM_PREFETCH, &control));

    return AM_HAL_STATUS_SUCCESS;
}

void nsx_power_platform_deep_sleep(void) {
    am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
}
