#include "nsx_psram.h"

const nsx_core_api_t nsx_psram_V0_0_1 = {.apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_V0_0_1};
const nsx_core_api_t nsx_psram_oldest_supported_version = {
    .apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_OLDEST_SUPPORTED_VERSION};
const nsx_core_api_t nsx_psram_current_version = {
    .apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_CURRENT_VERSION};

extern uint32_t nsx_psram_platform_init(nsx_psram_config_t *cfg);

uint32_t nsx_psram_default_config(nsx_psram_config_t *cfg) {
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    *cfg = (nsx_psram_config_t){
        .api = &nsx_psram_current_version,
        .enable = true,
        .enable_xip = true,
        .configure_mpu = true,
        .clock_freq = AM_HAL_MSPI_CLK_48MHZ,
        .nbtxn_buf = NULL,
        .nbtxn_buf_len = 0,
        .scrambling_start_addr = 0,
        .scrambling_end_addr = 0,
        .base_address = 0,
        .size_bytes = 0,
    };

    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_psram_init(nsx_psram_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(cfg->api, &nsx_psram_oldest_supported_version,
                          &nsx_psram_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    if (cfg == NULL || !cfg->enable) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    return nsx_psram_platform_init(cfg);
}
