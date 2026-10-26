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

#if defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
/* Preserve the BSP's SWO preparation after NSX's broader DCU preparation.
 * The vendor helper has additional boot/scope rules; it is not redundant. */
static uint32_t nsx_platform_bsp_swo_prepare(void) {
    uint32_t status = 0, cleanup_status;
    bool off_otp = false, off_crypto = false;
    AM_CRITICAL_BEGIN;
    if (NSX_PWRCTRL_PWRSTOTP == 0) {
        off_otp = true;
        status = am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_OTP);
        if (status != 0) goto cleanup;
    }
    if (NSX_PWRCTRL_PWRSTCRYPTO == 0) {
        off_crypto = true;
        status = am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_CRYPTO);
        if (status != 0) goto cleanup;
    }
    status = am_hal_dcu_swo_enable();
cleanup:
    if (off_crypto) {
        cleanup_status = am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_CRYPTO);
        if (status == 0) status = cleanup_status;
    }
    if (off_otp) {
        cleanup_status = am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_OTP);
        if (status == 0) status = cleanup_status;
    }
    AM_CRITICAL_END;
    return status;
}
#endif

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
 *   - Apollo330P/510L: Checked HAL calls preserve BSP TPIU/ITM/pin
 *     ordering. Trace clock = HFRC 48 MHz.
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
        uint32_t status = nsx_platform_dcu_unlock_swo();
        if (status != 0) return status;

#if defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
        /* Avoid the BSP's fail-stop loops while preserving its HAL sequence.
         * Underlying HAL waits and unreported errors remain vendor limits. */
        status = nsx_platform_bsp_swo_prepare();
        if (status != 0) return status;
        status = am_hal_tpiu_enable(AM_HAL_TPIU_BAUD_1M);
        if (status != 0) return status;
        status = am_hal_itm_enable();
        if (status != 0) return status;
        status = am_hal_gpio_pinconfig(AM_BSP_GPIO_ITM_SWO, g_AM_BSP_GPIO_ITM_SWO);
        if (status != 0) return status;
        am_util_stdio_printf_init((am_util_stdio_print_char_t)am_hal_itm_print);
#else
        /* Steps 2-3: Manual TPIU + ITM + SWO pin + printf.
         * Trace clock = HFRC_96MHz (fixed, independent of CPU perf mode).
         * JLink SWO viewer: -cpufreq 96000000 -swofreq 1000000 */
        status = am_hal_debug_enable();
        if (status != 0) return status;
        uint32_t swo_scaler = (96000000u / 1000000u) - 1;  /* 95 → 1 MHz SWO baud */
        status = am_hal_tpiu_config(
            MCUCTRL_DBGCTRL_DBGTPIUCLKSEL_HFRC_96MHz,
            0,
            TPI_CSPSR_CWIDTH_1BIT,
            TPI_SPPR_TXMODE_UART,
            swo_scaler);
        if (status != 0) return status;

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
        status = am_hal_gpio_pinconfig(AM_BSP_GPIO_ITM_SWO, g_AM_BSP_GPIO_ITM_SWO);
        if (status != 0) return status;

        am_util_stdio_printf_init((am_util_stdio_print_char_t)am_hal_itm_print);
#endif
    } else if (cfg->transport == NSX_DEBUG_UART) {
        return am_bsp_uart_printf_enable();
    }

    return 0;
}

uint32_t nsx_platform_debug_disable(nsx_debug_transport_e transport) {
    if (transport == NSX_DEBUG_UART) {
        am_bsp_uart_printf_disable();
        return 0;
    }
    if (transport != NSX_DEBUG_ITM) return 0;
#if defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    uint32_t status = am_hal_itm_disable();
    if (status != 0) return status;
    status = am_hal_tpiu_disable();
    if (status != 0) return status;
    am_util_stdio_printf_init(0);
    return am_hal_gpio_pinconfig(AM_BSP_GPIO_ITM_SWO, am_hal_gpio_pincfg_default);
#else
    /* Pair the manual enable above; BSP transport state was never set.
     * Preserve the BSP's ITM/pin/TPIU ordering and SWO high-level handling. */
    uint32_t status = am_hal_itm_disable();
    if (status != 0) return status;
    status = am_hal_gpio_state_write(AM_BSP_GPIO_ITM_SWO, AM_HAL_GPIO_OUTPUT_SET);
    if (status != 0) return status;
    status = am_hal_gpio_pinconfig(AM_BSP_GPIO_ITM_SWO, am_hal_gpio_pincfg_output);
    if (status != 0) return status;
    status = am_hal_tpiu_disable();
    if (status != 0) return status;
    am_util_stdio_printf_init(0);
    return am_hal_gpio_pinconfig(AM_BSP_GPIO_ITM_SWO, am_hal_gpio_pincfg_default);
#endif
}
