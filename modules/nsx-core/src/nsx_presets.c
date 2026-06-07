/**
 * @file nsx_presets.c
 * @brief Opinionated, opt-in system preset definitions.
 *
 * See nsx_presets.h. These are convenience configurations only; the core
 * system library does not depend on them.
 */

#include "nsx_presets.h"

const nsx_system_config_t nsx_system_development = {
    .perf_mode        = NSX_PERF_HIGH,
    .enable_cache     = true,
    .enable_sram      = true,
    .debug            = { .transport = NSX_DEBUG_ITM },
    .skip_bsp_init    = false,
    .spot_mgr_profile = true,
};

const nsx_system_config_t nsx_system_inference = {
    .perf_mode        = NSX_PERF_HIGH,
    .enable_cache     = true,
    .enable_sram      = false,
    .debug            = { .transport = NSX_DEBUG_NONE },
    .skip_bsp_init    = false,
    .spot_mgr_profile = true,
};

const nsx_system_config_t nsx_system_minimal = {
    .perf_mode        = NSX_PERF_LOW,
    .enable_cache     = false,
    .enable_sram      = false,
    .debug            = { .transport = NSX_DEBUG_NONE },
    .skip_bsp_init    = true,
    .spot_mgr_profile = false,
};
