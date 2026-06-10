/**
 * @file nsx_system.c
 * @brief Modular system initialization — platform-independent logic.
 *
 * Per-SoC backends are in src/apollo{soc}/nsx_system_platform.c.
 * This file contains the sequencing logic and preset definitions.
 */

#include "nsx_system.h"
#include "nsx_core.h"
#include "nsx_mem.h"
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"

/* ===================================================================
 * Platform backends (implemented per-SoC)
 * =================================================================== */

extern uint32_t nsx_platform_hw_init(void);
extern uint32_t nsx_platform_minimal_hw_init(void);
extern uint32_t nsx_platform_set_perf_mode(nsx_perf_mode_e mode);
extern uint32_t nsx_platform_debug_init(const nsx_debug_config_t *cfg);
extern uint32_t nsx_platform_spot_mgr_profile(void);

/* ===================================================================
 * API implementation
 * =================================================================== */

uint32_t nsx_hw_init(void) {
    return nsx_platform_hw_init();
}

uint32_t nsx_minimal_hw_init(void) {
    return nsx_platform_minimal_hw_init();
}

uint32_t nsx_set_perf_mode(nsx_perf_mode_e mode) {
    return nsx_platform_set_perf_mode(mode);
}

uint32_t nsx_debug_init(const nsx_debug_config_t *cfg) {
    if (cfg == NULL || cfg->transport == NSX_DEBUG_NONE) {
        return 0;
    }
    return nsx_platform_debug_init(cfg);
}

uint32_t nsx_system_init(const nsx_system_config_t *cfg) {
    uint32_t st;

    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    /* 1. Core runtime */
    nsx_core_config_t core_cfg = { .api = &nsx_core_V1_0_0 };
    st = nsx_core_init(&core_cfg);
    if (st != 0) return st;

    /* 2. Hardware init */
    if (cfg->skip_bsp_init) {
        st = nsx_minimal_hw_init();
    } else {
        st = nsx_hw_init();
    }
    if (st != 0) return st;

    /* 3. Cache (safe after step 2 set up CPDLP) */
    if (cfg->enable_cache) {
        st = nsx_cache_enable();
        if (st != 0) return st;
    }

    /* 4. Debug output.
     *
     * MUST run before the performance-mode switch.  Enabling SWO/ITM
     * requires temporarily powering up Crypto (to unlock the DCU), and the
     * Crypto power-up handshake requests HFRC from the clock manager.  On
     * Apollo5 secure parts this hangs if the CPU has already been switched
     * to a SYSPLL-sourced high-performance clock — so unlock the DCU while
     * still running on HFRC, then raise the performance mode.  As a bonus,
     * printf is available during perf-mode / SpotManager bring-up. */
    if (cfg->debug.transport != NSX_DEBUG_NONE) {
        st = nsx_debug_init(&cfg->debug);
        if (st != 0) return st;
    }

    /* 5. Performance mode */
    st = nsx_set_perf_mode(cfg->perf_mode);
    if (st != 0) return st;

    /* 6. SpotManager profile */
    if (cfg->spot_mgr_profile) {
        nsx_platform_spot_mgr_profile();
    }

    return 0;
}
