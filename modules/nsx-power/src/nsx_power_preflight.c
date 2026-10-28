/* Shared Apollo5-class startup preflight; no hardware effects. */
#include "nsx_power.h"

uint32_t nsx_power_platform_validate_config(const nsx_power_config_t *cfg) {
    /* API/version checks belong to the public wrapper, before ownership. */
    switch (cfg->perf_mode) {
    case NSX_POWER_PERF_NOT_SET:
    case NSX_POWER_PERF_LOW:
    case NSX_POWER_PERF_HIGH:
    case NSX_POWER_PERF_MAX:
        return NSX_STATUS_SUCCESS;
    default:
        return NSX_STATUS_INVALID_CONFIG;
    }
}
