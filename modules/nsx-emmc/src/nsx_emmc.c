#include "nsx_emmc.h"

const nsx_core_api_t nsx_emmc_V0_0_1 = {.apiId = NSX_EMMC_API_ID, .version = NSX_EMMC_V0_0_1};
const nsx_core_api_t nsx_emmc_oldest_supported_version = {
    .apiId = NSX_EMMC_API_ID, .version = NSX_EMMC_OLDEST_SUPPORTED_VERSION};
const nsx_core_api_t nsx_emmc_current_version = {
    .apiId = NSX_EMMC_API_ID, .version = NSX_EMMC_CURRENT_VERSION};

extern uint32_t nsx_emmc_platform_init(nsx_emmc_config_t *cfg);
extern uint32_t nsx_emmc_platform_block_write(uint32_t start_block, uint32_t num_blocks,
                                               const uint8_t *buf);
extern uint32_t nsx_emmc_platform_block_read(uint32_t start_block, uint32_t num_blocks,
                                              uint8_t *buf);

uint32_t nsx_emmc_default_config(nsx_emmc_config_t *cfg) {
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    *cfg = (nsx_emmc_config_t){
        .api = &nsx_emmc_current_version,
        .enable = true,
        .sdio_instance = 0,
        .bus_width = NSX_EMMC_BUS_WIDTH_4,
        .clock_freq_hz = 48000000u,
        .block_count = 0,
    };

    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_emmc_init(nsx_emmc_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(cfg->api, &nsx_emmc_oldest_supported_version,
                          &nsx_emmc_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    if (cfg == NULL || !cfg->enable) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    return nsx_emmc_platform_init(cfg);
}

uint32_t nsx_emmc_block_write(uint32_t start_block, uint32_t num_blocks, const uint8_t *buf) {
    if (buf == NULL || num_blocks == 0) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    return nsx_emmc_platform_block_write(start_block, num_blocks, buf);
}

uint32_t nsx_emmc_block_read(uint32_t start_block, uint32_t num_blocks, uint8_t *buf) {
    if (buf == NULL || num_blocks == 0) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    return nsx_emmc_platform_block_read(start_block, num_blocks, buf);
}
