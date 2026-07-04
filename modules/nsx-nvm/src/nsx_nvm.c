#include "nsx_nvm.h"

const nsx_core_api_t nsx_nvm_V0_0_1 = {.apiId = NSX_NVM_API_ID, .version = NSX_NVM_V0_0_1};
const nsx_core_api_t nsx_nvm_oldest_supported_version = {
    .apiId = NSX_NVM_API_ID, .version = NSX_NVM_OLDEST_SUPPORTED_VERSION};
const nsx_core_api_t nsx_nvm_current_version = {
    .apiId = NSX_NVM_API_ID, .version = NSX_NVM_CURRENT_VERSION};

extern uint32_t nsx_nvm_platform_init(nsx_nvm_config_t *cfg);
extern uint32_t nsx_nvm_platform_read(uint32_t addr, uint8_t *buf, uint32_t len, bool wait);
extern uint32_t nsx_nvm_platform_write(uint32_t addr, const uint8_t *buf, uint32_t len, bool wait);
extern uint32_t nsx_nvm_platform_sector_erase(uint32_t sector_addr);
extern uint32_t nsx_nvm_platform_mass_erase(void);
extern uint32_t nsx_nvm_platform_enable_xip(void);
extern uint32_t nsx_nvm_platform_disable_xip(void);

uint32_t nsx_nvm_default_config(nsx_nvm_config_t *cfg) {
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    *cfg = (nsx_nvm_config_t){
        .api = &nsx_nvm_current_version,
        .enable = true,
        .enable_xip = false,
        .chip_select = 0,
        .iface = NSX_NVM_IF_OCTAL,
        .clock_freq = AM_HAL_MSPI_CLK_48MHZ,
        .nbtxn_buf = NULL,
        .nbtxn_buf_len = 0,
        .scrambling_start_addr = 0,
        .scrambling_end_addr = 0,
        .configure_mpu = true,
        .xip_base_address = 0,
        .size_bytes = 0,
    };

    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_nvm_init(nsx_nvm_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(cfg->api, &nsx_nvm_oldest_supported_version,
                          &nsx_nvm_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    if (cfg == NULL || !cfg->enable) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    return nsx_nvm_platform_init(cfg);
}

uint32_t nsx_nvm_read(uint32_t addr, uint8_t *buf, uint32_t len, bool wait) {
    if (buf == NULL || len == 0) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    return nsx_nvm_platform_read(addr, buf, len, wait);
}

uint32_t nsx_nvm_write(uint32_t addr, const uint8_t *buf, uint32_t len, bool wait) {
    if (buf == NULL || len == 0) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    return nsx_nvm_platform_write(addr, buf, len, wait);
}

uint32_t nsx_nvm_sector_erase(uint32_t sector_addr) {
    return nsx_nvm_platform_sector_erase(sector_addr);
}

uint32_t nsx_nvm_mass_erase(void) {
    return nsx_nvm_platform_mass_erase();
}

uint32_t nsx_nvm_enable_xip(void) {
    return nsx_nvm_platform_enable_xip();
}

uint32_t nsx_nvm_disable_xip(void) {
    return nsx_nvm_platform_disable_xip();
}
