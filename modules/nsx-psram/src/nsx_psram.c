#include "nsx_psram.h"
#include "nsx_psram_internal.h"

const nsx_core_api_t nsx_psram_V1_0_0 = {.apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_V1_0_0};
const nsx_core_api_t nsx_psram_oldest_supported_version = {
    .apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_OLDEST_SUPPORTED_VERSION};
const nsx_core_api_t nsx_psram_current_version = {
    .apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_CURRENT_VERSION};

#ifndef NSX_PSRAM_PLATFORM_CAPABILITIES
#define NSX_PSRAM_PLATFORM_CAPABILITIES NSX_PSRAM_CAP_SYNC_TRANSFER
#endif

static nsx_psram_info_t g_nsx_psram_info = {
    .capabilities = NSX_PSRAM_PLATFORM_CAPABILITIES,
    .state = NSX_PSRAM_STATE_UNINITIALIZED,
    .timing_status = NSX_PSRAM_TIMING_UNAVAILABLE,
};

static uint32_t nsx_psram_validate_transfer(
    uint32_t offset, const void *buffer, uint32_t length) {
    if (g_nsx_psram_info.state == NSX_PSRAM_STATE_FAILED) {
        return NSX_PSRAM_STATUS_FAILED_STATE;
    }
    if (g_nsx_psram_info.state != NSX_PSRAM_STATE_READY) {
        return NSX_PSRAM_STATUS_NOT_INITIALIZED;
    }
    if (length == 0u) {
        return NSX_STATUS_SUCCESS;
    }
    if (buffer == NULL) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (offset >= g_nsx_psram_info.size_bytes ||
        length > g_nsx_psram_info.size_bytes - offset) {
        return NSX_PSRAM_STATUS_OUT_OF_RANGE;
    }
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_psram_default_config(nsx_psram_config_t *cfg) {
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    *cfg = (nsx_psram_config_t){
        .api = &nsx_psram_current_version,
        .clock_hz = 48000000u,
        .enable_xip = true,
        .configure_mpu = true,
    };

    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_psram_init(const nsx_psram_config_t *cfg) {
    nsx_psram_platform_info_t platform_info = {
        .safe_to_retry = true,
        .timing_status = NSX_PSRAM_TIMING_UNAVAILABLE,
    };
    uint32_t status;

#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(cfg->api, &nsx_psram_oldest_supported_version,
                          &nsx_psram_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    if (cfg == NULL || cfg->clock_hz == 0u) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (g_nsx_psram_info.state == NSX_PSRAM_STATE_READY) {
        return NSX_PSRAM_STATUS_ALREADY_INITIALIZED;
    }
    if (g_nsx_psram_info.state == NSX_PSRAM_STATE_FAILED) {
        return NSX_PSRAM_STATUS_FAILED_STATE;
    }

    status = nsx_psram_platform_init(cfg, &platform_info);
    g_nsx_psram_info = (nsx_psram_info_t){
        .base_address = platform_info.base_address,
        .size_bytes = platform_info.size_bytes,
        .configured_clock_hz = platform_info.clock_hz,
        .capabilities = NSX_PSRAM_PLATFORM_CAPABILITIES,
        .last_init_status = status,
        .state = status == NSX_STATUS_SUCCESS
            ? NSX_PSRAM_STATE_READY
            : (platform_info.safe_to_retry
                ? NSX_PSRAM_STATE_UNINITIALIZED
                : NSX_PSRAM_STATE_FAILED),
        .xip_enabled = platform_info.xip_enabled,
        .timing_status = platform_info.timing_status,
        .rxdqs_delay = platform_info.rxdqs_delay,
    };
    return status;
}

uint32_t nsx_psram_get_info(nsx_psram_info_t *info) {
    if (info == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    *info = g_nsx_psram_info;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_psram_read(uint32_t offset, void *buffer, uint32_t length) {
    uint32_t status = nsx_psram_validate_transfer(offset, buffer, length);
    if (status != NSX_STATUS_SUCCESS) {
        return status;
    }
    if (length == 0u) {
        return NSX_STATUS_SUCCESS;
    }
    return nsx_psram_platform_read(offset, buffer, length);
}

uint32_t nsx_psram_write(uint32_t offset, const void *buffer, uint32_t length) {
    uint32_t status = nsx_psram_validate_transfer(offset, buffer, length);
    if (status != NSX_STATUS_SUCCESS) {
        return status;
    }
    if (length == 0u) {
        return NSX_STATUS_SUCCESS;
    }
    return nsx_psram_platform_write(offset, buffer, length);
}
