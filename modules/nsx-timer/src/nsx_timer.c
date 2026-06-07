/**
 * @file nsx_timer.c
 * @author Ambiq
 * @brief Simple timer facility
 * @version 0.1
 * @date 2022-10-11
 *
 * @copyright Copyright (c) 2022-2026, Ambiq Micro, Inc.
 *
 */

#include "nsx_timer.h"
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"

const nsx_core_api_t nsx_timer_V0_0_1 = {.apiId = NSX_TIMER_API_ID, .version = NSX_TIMER_V0_0_1};

const nsx_core_api_t nsx_timer_V1_0_0 = {.apiId = NSX_TIMER_API_ID, .version = NSX_TIMER_V1_0_0};

const nsx_core_api_t nsx_timer_oldest_supported_version = {
    .apiId = NSX_TIMER_API_ID, .version = NSX_TIMER_V0_0_1};

const nsx_core_api_t nsx_timer_current_version = {
    .apiId = NSX_TIMER_API_ID, .version = NSX_TIMER_V1_0_0};

nsx_timer_config_t *nsx_timer_config[NSX_TIMER_TEMPCO + 1];

extern uint32_t nsx_timer_platform_init(nsx_timer_config_t *cfg);

uint32_t nsx_timer_init(nsx_timer_config_t *cfg) {
    uint32_t ui32Status = AM_HAL_STATUS_SUCCESS;

#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(
            cfg->api, &nsx_timer_oldest_supported_version, &nsx_timer_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }

    if (cfg->timer > NSX_TIMER_TEMPCO) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if ((cfg->enableInterrupt) && (cfg->callback == NULL)) {
        return NSX_STATUS_INVALID_CONFIG;
    }
#endif

    nsx_timer_config[cfg->timer] = cfg;

    ui32Status = nsx_timer_platform_init(cfg);

    return ui32Status;
}
