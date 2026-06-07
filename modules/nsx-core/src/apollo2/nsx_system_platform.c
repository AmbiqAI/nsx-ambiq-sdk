/**
 * @file nsx_system_platform.c
 * @brief Apollo2-family platform backend for nsx_system.h.
 *
 * Covers: Apollo2.
 * Key differences from later Ambiq families:
 *   - Cortex-M4F cache and FPU setup
 *   - Apollo2's cache controller is enabled with a single call that takes a
 *     config pointer (am_hal_cachectrl_enable(const am_hal_cachectrl_config_t*)),
 *     unlike Apollo3 which splits config and enable into two calls.
 *   - No CPDLP, no SpotManager
 *   - No Apollo4-style MCU performance mode selector
 *   - The Apollo2 BSP exposes am_bsp_debug_printf_enable() for ITM/SWO setup
 *     (it has no am_bsp_itm_printf_enable / am_bsp_uart_printf_enable helpers).
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
 * nsx_platform_minimal_hw_init — lightweight init for Apollo2
 *
 * Apollo2 doesn't need CPDLP or SpotManager. Enable caching and FPU.
 * =================================================================== */

uint32_t nsx_platform_minimal_hw_init(void) {
    /* Enable cache with default config (single-call API on Apollo2). */
    am_hal_cachectrl_enable(&am_hal_cachectrl_defaults);

    /* FPU */
    am_hal_sysctrl_fpu_enable();
    am_hal_sysctrl_fpu_stacking_enable(true);

    return 0;
}

/* ===================================================================
 * nsx_platform_set_perf_mode — no-op on Apollo2
 * =================================================================== */

uint32_t nsx_platform_set_perf_mode(nsx_perf_mode_e mode) {
    (void)mode;
    return 0;
}

/* ===================================================================
 * nsx_platform_spot_mgr_profile — no-op on Apollo2
 * =================================================================== */

uint32_t nsx_platform_spot_mgr_profile(void) {
    return 0;
}

/* ===================================================================
 * nsx_platform_debug_init — ITM/SWO setup for Apollo2
 *
 * Apollo2's BSP provides a single am_bsp_debug_printf_enable() helper that
 * brings up the default debug transport (ITM/SWO).
 * =================================================================== */

uint32_t nsx_platform_debug_init(const nsx_debug_config_t *cfg) {
    if (cfg == NULL) return 0;

    if (cfg->transport == NSX_DEBUG_ITM || cfg->transport == NSX_DEBUG_UART) {
        am_bsp_debug_printf_enable();
    }

    return 0;
}
