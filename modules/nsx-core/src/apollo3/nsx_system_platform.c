/**
 * @file nsx_system_platform.c
 * @brief Apollo3-family platform backend for nsx_system.h.
 *
 * Covers: Apollo3 and Apollo3P.
 * Key differences from later Ambiq families:
 *   - Cortex-M4F cache and FPU setup
 *   - No CPDLP, no SpotManager
 *   - No Apollo4-style MCU performance mode selector
 *   - BSP helper am_bsp_itm_printf_enable() handles ITM/SWO setup
 */

#include "nsx_system.h"
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"

/* ===================================================================
 * nsx_platform_hw_init — full BSP low-power init
 * =================================================================== */

uint32_t nsx_platform_hw_init(void) {
    am_bsp_low_power_init();
    return 0;
}

/* ===================================================================
 * nsx_platform_minimal_hw_init — lightweight init for Apollo3
 *
 * Apollo3 doesn't need CPDLP or SpotManager. Enable caching and FPU.
 * =================================================================== */

uint32_t nsx_platform_minimal_hw_init(void) {
    /* Enable cache with default config. */
    am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
    am_hal_cachectrl_enable();

    /* FPU */
    am_hal_sysctrl_fpu_enable();
    am_hal_sysctrl_fpu_stacking_enable(true);

    return 0;
}

/* ===================================================================
 * nsx_platform_set_perf_mode — no-op on Apollo3
 * =================================================================== */

uint32_t nsx_platform_set_perf_mode(nsx_perf_mode_e mode) {
    (void)mode;
    return 0;
}

/* ===================================================================
 * nsx_platform_spot_mgr_profile — no-op on Apollo3
 * =================================================================== */

uint32_t nsx_platform_spot_mgr_profile(void) {
    return 0;
}

/* ===================================================================
 * nsx_platform_debug_init — ITM/SWO setup for Apollo3
 * =================================================================== */

uint32_t nsx_platform_debug_init(const nsx_debug_config_t *cfg) {
    if (cfg == NULL) return 0;

    if (cfg->transport == NSX_DEBUG_ITM) {
        am_bsp_itm_printf_enable();
    } else if (cfg->transport == NSX_DEBUG_UART) {
        am_bsp_uart_printf_enable();
    }

    return 0;
}
