/* Power startup checks that run before any hardware effects. */
#include "nsx_power.h"

uint32_t nsx_power_platform_validate_config(const nsx_power_config_t *cfg) {
    /* API/version checks belong to the public wrapper, before ownership. */
#if defined(PART_atomiq110)
    // All packaged AT110 layouts can use upper DTCM and shared SRAM.
    if (cfg->small_tcm || !cfg->need_ssram) return NSX_STATUS_INVALID_CONFIG;
#endif
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
