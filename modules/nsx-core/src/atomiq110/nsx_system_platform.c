/**
 * @file nsx_system_platform.c
 * @brief atomiq110 platform backend for nsx_system.h.
 *
 * Covers: atomiq110 (the only upstream realization today is the FPGA
 * "turbo" board, atomiq110_fpga_turbo). Implements the platform-specific
 * helpers called by nsx_system.c.
 *
 * atomiq110 is close to but not register-compatible with Apollo510, so it
 * keeps its own arch dir. The parts that ARE identical — the DCU unlock
 * critical section and the hw-init / perf-mode helpers — are not duplicated
 * here: they come from the shared Apollo5-class body
 * ../nsx_system_platform_apollo5.inc (which also supplies the common
 * #includes), parameterized by the register aliases below. That keeps the
 * delicate crypto/OTP unlock handshake in exactly one place.
 *
 * What remains below is what is genuinely atomiq110-specific:
 * nsx_platform_spot_mgr_profile() (no-op here) and nsx_platform_debug_init()
 * (ITM bring-up delegated to the BSP's am_bsp_itm_printf_enable()).
 */

/* atomiq110 splits the single Apollo510-style DEVPWRSTATUS register into
 * DEVPWRSTATUS0/DEVPWRSTATUS1; PWRSTOTP/PWRSTCRYPTO live in DEVPWRSTATUS0.
 * This is the only functional delta from the Apollo510 realization of the
 * shared body, so it is expressed as an accessor override at the include
 * site rather than a part #if inside the shared file. */
#define NSX_PWRCTRL_PWRSTOTP    (PWRCTRL->DEVPWRSTATUS0_b.PWRSTOTP)
#define NSX_PWRCTRL_PWRSTCRYPTO (PWRCTRL->DEVPWRSTATUS0_b.PWRSTCRYPTO)

#include "../nsx_system_platform_apollo5.inc"

/* ===================================================================
 * nsx_platform_spot_mgr_profile
 *
 * atomiq110's SpotManager is stimulus/state driven
 * (am_hal_spotmgr_power_state_update()) with no STM/STMP "collapse" profile
 * concept that am_hal_spotmgr_profile_set() exposes on Apollo510/5A/5B, so
 * this is intentionally a no-op here.
 * =================================================================== */

uint32_t nsx_platform_spot_mgr_profile(void) {
    return 0;
}

/* ===================================================================
 * nsx_platform_debug_init — ITM/SWO setup for atomiq110
 *
 * ITM bring-up is delegated to the BSP's am_bsp_itm_printf_enable(),
 * which performs, in order:
 *
 *   1. DCU unlock for SWO (OTP/Crypto power handshake + am_hal_dcu_*)
 *   2. am_hal_tpiu_enable(AM_HAL_TPIU_BAUD_1M)
 *   3. am_hal_itm_enable()
 *   4. SWO pin configuration (AM_BSP_GPIO_ITM_SWO)
 *   5. am_util_stdio_printf_init(am_hal_itm_print)
 *
 * The TPIU-before-ITM ordering is a hard requirement of this HAL, not a
 * stylistic choice: am_hal_tpiu_enable() only records the requested ITM
 * baud (via am_hal_itm_parameters_set()), and it is am_hal_itm_enable()
 * that computes and programs the SWO scaler from the recorded value.
 * Enabling ITM first silently discards the requested baud -- the scaler
 * falls back to AM_HAL_TPIU_BAUD_DEFAULT. Delegating to the BSP keeps
 * this backend on the single maintained bring-up sequence instead of
 * restating (and risking divergence from) it here.
 *
 * The only atomiq110 realization today is the FPGA "turbo" board, whose
 * HFRC is fixed at the ATOMIQ11X_FPGA emulation frequency (25 MHz,
 * am_mcu_apollo.h), independent of CPU perf mode, matching
 * NSX_SEGGER_CPUFREQ in cmake/socs/facts/atomiq110.cmake.
 * am_hal_itm_enable() accounts for the FPGA's DIV10 clocks internally
 * when computing the SWO scaler for the 1 MHz baud.
 *
 * The JLink SWO viewer must be told the *trace clock* frequency (not CPU
 * clock) via -cpufreq so that its ACPR override matches:
 *   JLink SWO viewer: -cpufreq 25000000 -swofreq 1000000
 * =================================================================== */

uint32_t nsx_platform_debug_init(const nsx_debug_config_t *cfg) {
    if (cfg == NULL) return 0;

    if (cfg->transport == NSX_DEBUG_ITM) {
        /* DCU unlock, TPIU @ 1 MHz, ITM, SWO pin, printf backend -- see
         * the ordering requirement documented above. */
        return (uint32_t)am_bsp_itm_printf_enable();
    } else if (cfg->transport == NSX_DEBUG_UART) {
        return am_bsp_uart_printf_enable();
    }

    return 0;
}
