/**
 * @file nsx_system_platform.c
 * @brief Apollo5-family platform backend for nsx_system.h.
 *
 * Covers: Apollo510, Apollo510B, Apollo510L, Apollo5A, Apollo5B, Apollo330P.
 * Implements the platform-specific helpers called by nsx_system.c.
 *
 * The DCU unlock and the hw-init / perf-mode helpers are identical on every
 * Apollo5-class part (including atomiq110) and therefore live in the shared
 * body ../nsx_system_platform_apollo5.inc, which also supplies the common
 * #includes. Only the genuinely part-specific pieces —
 * nsx_platform_spot_mgr_profile() and nsx_platform_debug_init() — are below.
 *
 * These parts use the single-bank PWRCTRL->DEVPWRSTATUS register, which is the
 * shared body's default, so no register aliases are defined here.
 */

#include "../nsx_system_platform_apollo5.inc"

/* ===================================================================
 * nsx_platform_spot_mgr_profile
 * =================================================================== */

uint32_t nsx_platform_spot_mgr_profile(void) {
#if defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || \
    defined(AM_PART_APOLLO5A) || defined(AM_PART_APOLLO5B)
    /* am_hal_spotmgr_profile_set() is not available on all R5 SDK
     * revisions (e.g. Apollo330P r5.2-alpha). Guard by part define. */
    am_hal_spotmgr_profile_t profile;
    profile.PROFILE = 0;
    profile.PROFILE_b.COLLAPSESTMANDSTMP = 1;
    am_hal_spotmgr_profile_set(&profile);
#endif
    return 0;
}

/* ===================================================================
 * nsx_platform_debug_init — ITM/SWO setup for Apollo5 family
 *
 * All Apollo5 variants share these steps:
 *   1. DCU unlock (OTP + Crypto → am_hal_dcu_update → power down)
 *   2. TPIU / ITM / SWO pin configuration
 *   3. printf backend registration
 *
 * Step 2-3 differ by variant:
 *   - Apollo330P/510L: BSP helper (am_bsp_itm_printf_enable) handles
 *     TPIU + ITM + SWO pin + printf. Trace clock = XTAL_HS 48 MHz.
 *   - Apollo510/5B/5A: Manual TPIU config with HFRC_96MHz because
 *     MCUCTRL_DBGCTRL_DBGTPIUCLKSEL is only available on these parts.
 *     SWO pin (GPIO 28) is configured via the BSP-supplied
 *     g_AM_BSP_GPIO_ITM_SWO pincfg (defined in libam_bsp.a's .data,
 *     so it is valid before any BSP entrypoint runs).
 *
 * The JLink SWO viewer must be told the *trace clock* frequency
 * (not CPU clock) via -cpufreq so that its ACPR override matches.
 * See cmake/socs/facts/<soc>.cmake for the per-SoC NSX_SEGGER_CPUFREQ values.
 * =================================================================== */

uint32_t nsx_platform_debug_init(const nsx_debug_config_t *cfg) {
    if (cfg == NULL) return 0;

    if (cfg->transport == NSX_DEBUG_ITM) {
        /* Step 1: Unlock DCU (common to all Apollo5 variants) */
        nsx_platform_dcu_unlock_swo();

#if defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
        /* Steps 2-3: BSP helper handles TPIU/ITM/SWO pin/printf.
         * Trace clock on these parts = XTAL_HS 48 MHz.
         * JLink SWO viewer: -cpufreq 48000000 -swofreq 1000000 */
    return am_bsp_itm_printf_enable();
#else
        /* Steps 2-3: Manual TPIU + ITM + SWO pin + printf.
         * Trace clock = HFRC_96MHz (fixed, independent of CPU perf mode).
         * JLink SWO viewer: -cpufreq 96000000 -swofreq 1000000 */
        am_hal_debug_enable();
        uint32_t swo_scaler = (96000000u / 1000000u) - 1;  /* 95 → 1 MHz SWO baud */
        am_hal_tpiu_config(
            MCUCTRL_DBGCTRL_DBGTPIUCLKSEL_HFRC_96MHz,
            0,
            TPI_CSPSR_CWIDTH_1BIT,
            TPI_SPPR_TXMODE_UART,
            swo_scaler);

        ITM->TPR = 0xFFFFFFFF;
        ITM->TER = 0xFFFFFFFF;
        ITM->TCR =
            _VAL2FLD(ITM_TCR_SWOENA, 1) |
            _VAL2FLD(ITM_TCR_DWTENA, 1) |
            _VAL2FLD(ITM_TCR_SYNCENA, 1) |
            _VAL2FLD(ITM_TCR_ITMENA, 1);

        /* Use the BSP-supplied SWO pincfg (correct funcsel for AP510 GPIO 28).
         * The previous code constructed a pincfg with uFuncSel=0, which is
         * the GPIO function — not SWO — so SWO output never reached the pin
         * even though TPIU/ITM were configured correctly. */
        am_hal_gpio_pinconfig(AM_BSP_GPIO_ITM_SWO, g_AM_BSP_GPIO_ITM_SWO);

        am_util_stdio_printf_init((am_util_stdio_print_char_t)am_hal_itm_print);
#endif
    } else if (cfg->transport == NSX_DEBUG_UART) {
        return am_bsp_uart_printf_enable();
    }

    return 0;
}
