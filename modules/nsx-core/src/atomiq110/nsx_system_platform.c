/**
 * @file nsx_system_platform.c
 * @brief atomiq110 platform backend for nsx_system.h.
 *
 * Covers: atomiq110 (the only upstream realization today is the FPGA
 * "turbo" board, atomiq110_fpga_turbo). Implements the platform-specific
 * helpers called by nsx_system.c.
 *
 * This is a dedicated file rather than a shared branch inside the
 * Apollo5-family apollo510/nsx_system_platform.c: atomiq110 is close to but
 * not register-compatible with Apollo510 (see the DCU-unlock and TPIU
 * sections below), so keeping it separate avoids part-agnostic aliasing
 * inside code that's otherwise specific to shipping Apollo5 boards.
 */

#include "nsx_system.h"
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"

#include "am_hal_spotmgr.h"
#include "am_hal_dcu.h"

/* ===================================================================
 * DCU unlock
 *
 * The Secure Bootloader (SBL) locks the DCU before transferring control
 * to user code.  Re-enabling SWO/ITM requires temporarily powering up
 * OTP and Crypto, calling am_hal_dcu_update(), then shutting them down.
 * =================================================================== */

#define NSX_DCU_SWO_MASK (                                            \
    AM_HAL_DCU_CPUTRC_DWT_SWO | AM_HAL_DCU_CPUDBG_NON_INVASIVE |       \
    AM_HAL_DCU_CPUDBG_S_NON_INVASIVE | AM_HAL_DCU_CPUTRC_PERFCNT |     \
    AM_HAL_DCU_SWD | AM_HAL_DCU_TRACE)

static uint32_t nsx_platform_dcu_unlock_swo(void) {
    uint32_t ui32dcuVal;
    int32_t  i32RetValue = 0;
    bool     bOffCryptoOnExit = false;
    bool     bOffOtpOnExit = false;

    // The crypto/OTP power-up handshake (and the HAL's internal HFRC clock
    // request for crypto) must run uninterrupted.  An ISR that touches the
    // clock manager or a power domain mid-sequence can wedge the crypto core,
    // which on this secure part shows up as a hang inside
    // am_hal_pwrctrl_periph_enable(CRYPTO).  Match the proven neuralSPOT
    // sequence: do the whole thing in a critical section, only power up
    // domains that are currently off, and let the HAL do the idle-wait.
    AM_CRITICAL_BEGIN;

    // atomiq110 splits the single Apollo510-style DEVPWRSTATUS register into
    // DEVPWRSTATUS0/DEVPWRSTATUS1; PWRSTOTP/PWRSTCRYPTO live in DEVPWRSTATUS0.
    if (PWRCTRL->DEVPWRSTATUS0_b.PWRSTOTP == 0) {
        bOffOtpOnExit = true;
        am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_OTP);
    }

    if (PWRCTRL->DEVPWRSTATUS0_b.PWRSTCRYPTO == 0) {
        bOffCryptoOnExit = true;
        am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_CRYPTO);
    }

    if ((PWRCTRL->DEVPWRSTATUS0_b.PWRSTCRYPTO == 1) &&
        (CRYPTO->HOSTCCISIDLE_b.HOSTCCISIDLE == 1)) {
        am_hal_dcu_get(&ui32dcuVal);
        if (((ui32dcuVal & NSX_DCU_SWO_MASK) != NSX_DCU_SWO_MASK) &&
            (am_hal_dcu_update(true, NSX_DCU_SWO_MASK) != AM_HAL_STATUS_SUCCESS)) {
            i32RetValue = -1;
        }
    } else {
        i32RetValue = -1;
    }

    if (bOffCryptoOnExit) {
        am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_CRYPTO);
    }
    if (bOffOtpOnExit) {
        am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_OTP);
    }

    AM_CRITICAL_END;

    return (uint32_t)(i32RetValue == 0 ? 0 : 1);
}

/* ===================================================================
 * nsx_platform_hw_init — full BSP low-power init
 * =================================================================== */

uint32_t nsx_platform_hw_init(void) {
    am_bsp_low_power_init();
    return 0;
}

/* ===================================================================
 * nsx_platform_minimal_hw_init — fast startup, no BSP delay
 *
 * Subset of am_hal_pwrctrl_low_power_init() needed for:
 *   - CPDLP (cache power domain)
 *   - SpotManager init
 *   - FPU
 * =================================================================== */

uint32_t nsx_platform_minimal_hw_init(void) {
    /* CPDLP: enable cache power domain so icache/dcache enable won't fail.
     * am_hal_pwrctrl_low_power_init() does this internally. */
    am_hal_pwrctrl_pwrmodctl_cpdlp_t cpdlp = {
        .eRlpConfig = AM_HAL_PWRCTRL_RLP_ON,
        .eElpConfig = AM_HAL_PWRCTRL_ELP_ON,
        .eClpConfig = AM_HAL_PWRCTRL_CLP_ON
    };
    am_hal_pwrctrl_pwrmodctl_cpdlp_config(cpdlp);

    /* SpotManager must be initialized before profile_set */
    am_hal_spotmgr_init();

    /* FPU */
    am_hal_sysctrl_fpu_enable();
    am_hal_sysctrl_fpu_stacking_enable(true);

    return 0;
}

/* ===================================================================
 * nsx_platform_set_perf_mode
 * =================================================================== */

uint32_t nsx_platform_set_perf_mode(nsx_perf_mode_e mode) {
    if (mode == NSX_PERF_HIGH || mode == NSX_PERF_MEDIUM) {
        return am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE);
    } else {
        return am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER);
    }
}

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
 *   1. DCU unlock (OTP + Crypto → am_hal_dcu_update → power down)
 *   2. Manual TPIU / ITM / SWO pin configuration
 *   3. printf backend registration
 *
 * atomiq110 has its own TPIU clock-select field (CRM_TPIUCLKCFG, exposed
 * via the AM_HAL_TPIU_CLKSEL_* macros in am_hal_tpiu.h) which is distinct
 * from the MCUCTRL_DBGCTRL_DBGTPIUCLKSEL field used on Apollo510/5A/5B.
 *
 * CRM_TPIUCLKCFG bus fault (atomiq110_fpga_turbo):
 * am_hal_debug_enable() and am_hal_tpiu_config() both unconditionally
 * read-modify-write CRM_TPIUCLKCFG (CRM_BASE + 0x180) to select the trace
 * clock source. On this FPGA build that register is not reachable from
 * non-secure code: it precise-bus-faults (CFSR=0x8200, BFAR=0x40006180)
 * on a bare *read*, at reset, before any firmware runs, and remains
 * unreadable even after nsx_platform_dcu_unlock_swo() and an
 * am_hal_dcu_update() with every DCU bit set — so it is not gated by the
 * DCU trace-unlock mechanism, unlike the CoreSight TPIU/ITM registers
 * below. Since neither HAL/BSP source may change, this backend skips
 * am_hal_debug_enable()/am_hal_tpiu_config() entirely and instead
 * programs the standard ARM CoreSight TPIU registers (TPIU_BASE,
 * architectural — not an Ambiq HAL/BSP symbol) directly, leaving the
 * trace clock at its hardware reset default instead of explicitly
 * selecting HFRC via CRM_TPIUCLKCFG.
 *
 * The only atomiq110 realization today is the FPGA "turbo" board, whose
 * HFRC is fixed at the ATOMIQ11X_FPGA emulation frequency (25 MHz,
 * am_mcu_apollo.h), independent of CPU perf mode, matching
 * NSX_SEGGER_CPUFREQ in cmake/socs/facts/atomiq110.cmake.
 *
 * The JLink SWO viewer must be told the *trace clock* frequency (not CPU
 * clock) via -cpufreq so that its ACPR override matches:
 *   JLink SWO viewer: -cpufreq 25000000 -swofreq 1000000
 * =================================================================== */

uint32_t nsx_platform_debug_init(const nsx_debug_config_t *cfg) {
    if (cfg == NULL) return 0;

    if (cfg->transport == NSX_DEBUG_ITM) {
        /* Step 1: Unlock DCU */
        nsx_platform_dcu_unlock_swo();

        /* Steps 2-3: Manual TPIU + ITM + SWO pin + printf.
         *
         * Deliberately does NOT call am_hal_debug_enable() or
         * am_hal_tpiu_config(): both touch CRM_TPIUCLKCFG, which
         * bus-faults on this part/board (see comment above). The
         * CoreSight TPIU registers themselves (distinct from that CRM
         * clock-select register) are ordinary architectural registers
         * and are freely accessible once the DCU trace bits are unlocked. */
        /* DEMCR.TRCENA (DCB, architectural) gates DWT/ITM/PMU trace
         * generation. am_hal_debug_enable() normally sets this as part of
         * its (otherwise CRM-touching) bring-up; since that whole call is
         * skipped here, TRCENA must be set explicitly or the TPIU/ITM
         * produce corrupt/undefined SWO output instead of clean bytes. */
        DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;

        TPIU_Type *tpi = (TPIU_Type *)TPIU_BASE;
        uint32_t swo_scaler = (25000000u / 1000000u) - 1;  /* 24 → 1 MHz SWO baud */
        tpi->CSPSR = TPI_CSPSR_CWIDTH_1BIT;
        tpi->ACPR  = swo_scaler;
        tpi->SPPR  = TPI_SPPR_TXMODE_UART;

        ITM->TPR = 0xFFFFFFFF;
        ITM->TER = 0xFFFFFFFF;
        ITM->TCR =
            _VAL2FLD(ITM_TCR_SWOENA, 1) |
            _VAL2FLD(ITM_TCR_DWTENA, 1) |
            _VAL2FLD(ITM_TCR_SYNCENA, 1) |
            _VAL2FLD(ITM_TCR_ITMENA, 1);

        /* Same SWO pin (GPIO 28) and BSP pincfg symbol as Apollo510. */
        am_hal_gpio_pinconfig(AM_BSP_GPIO_ITM_SWO, g_AM_BSP_GPIO_ITM_SWO);

        am_util_stdio_printf_init((am_util_stdio_print_char_t)am_hal_itm_print);
    } else if (cfg->transport == NSX_DEBUG_UART) {
        return am_bsp_uart_printf_enable();
    }

    return 0;
}
