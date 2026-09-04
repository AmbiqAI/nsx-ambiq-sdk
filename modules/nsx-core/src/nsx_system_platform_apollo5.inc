/**
 * @file nsx_system_platform_apollo5.inc
 * @brief Shared Apollo5-class platform backend body for nsx_system.h.
 *
 * NOT a standalone translation unit: this file is textually included exactly
 * once by each arch-dir nsx_system_platform.c that implements an Apollo5-class
 * part:
 *
 *   src/apollo510/nsx_system_platform.c   (Apollo510/510B/510L/5A/5B/330P)
 *   src/atomiq110/nsx_system_platform.c   (atomiq110)
 *
 * It provides the pieces those parts implement identically:
 *
 *   nsx_platform_dcu_unlock_swo()   (static; used by the Apollo510
 *                                    nsx_platform_debug_init; atomiq110
 *                                    delegates to am_bsp_itm_printf_enable(),
 *                                    which performs its own DCU unlock)
 *   nsx_platform_hw_init()
 *   nsx_platform_minimal_hw_init()
 *   nsx_platform_set_perf_mode()
 *
 * The including file remains responsible for the pieces that are genuinely
 * part-specific: nsx_platform_spot_mgr_profile() and, above all,
 * nsx_platform_debug_init() (atomiq110 has its own TPIU clock-select field and
 * a CRM_TPIUCLKCFG bus-fault workaround that Apollo510 must not inherit).
 *
 * The DCU unlock below is a delicate crypto/OTP power handshake with
 * documented hang modes.  It lives here, in one place, precisely so that it
 * cannot drift between parts: any change to it must be made once and is then
 * shared by every Apollo5-class backend.
 *
 * ---- Part parameterization ----------------------------------------------
 *
 * The only functional delta between the Apollo510 and atomiq110 realizations
 * of this code is the power-status register bank.  Apollo510 exposes a single
 * PWRCTRL->DEVPWRSTATUS register; atomiq110 splits it into
 * DEVPWRSTATUS0/DEVPWRSTATUS1, with PWRSTOTP and PWRSTCRYPTO both landing in
 * DEVPWRSTATUS0.  Rather than gate on a part macro in here, the including
 * file may pre-define the two accessors below; the Apollo510 spelling is the
 * default, and an atomiq110-class (split-bank) part overrides it at the
 * include site like so:
 *
 *   #define NSX_PWRCTRL_PWRSTOTP    (PWRCTRL->DEVPWRSTATUS0_b.PWRSTOTP)
 *   #define NSX_PWRCTRL_PWRSTCRYPTO (PWRCTRL->DEVPWRSTATUS0_b.PWRSTCRYPTO)
 *   #include "../nsx_system_platform_apollo5.inc"
 *
 * (Overriding at the include site, rather than #if-ing on a part define in
 * here, keeps the register-bank fact next to the part it describes and keeps
 * this shared body closed to per-part fan-out. Note atomiq110's SoC facts
 * inject PART_atomiq110 but deliberately no AM_PART_* HAL gate, so an
 * AM_PART_ATOMIQ110 #if in here would silently evaluate false.)
 */

#include "nsx_system.h"
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h" /* also used by the includers' nsx_platform_debug_init() */

#include "am_hal_spotmgr.h"
#include "am_hal_dcu.h"

/* Power-status register aliases — see "Part parameterization" above. */
#ifndef NSX_PWRCTRL_PWRSTOTP
#define NSX_PWRCTRL_PWRSTOTP    (PWRCTRL->DEVPWRSTATUS_b.PWRSTOTP)
#endif
#ifndef NSX_PWRCTRL_PWRSTCRYPTO
#define NSX_PWRCTRL_PWRSTCRYPTO (PWRCTRL->DEVPWRSTATUS_b.PWRSTCRYPTO)
#endif

/* ===================================================================
 * DCU unlock — shared by all Apollo5-class parts
 *
 * The Secure Bootloader (SBL) locks the DCU before transferring control
 * to user code.  Re-enabling SWO/ITM requires temporarily powering up
 * OTP and Crypto, calling am_hal_dcu_update(), then shutting them down.
 * This sequence is identical across all Apollo5 variants.
 * =================================================================== */

#define NSX_DCU_SWO_MASK (                                            \
    AM_HAL_DCU_CPUTRC_DWT_SWO | AM_HAL_DCU_CPUDBG_NON_INVASIVE |       \
    AM_HAL_DCU_CPUDBG_S_NON_INVASIVE | AM_HAL_DCU_CPUTRC_PERFCNT |     \
    AM_HAL_DCU_SWD | AM_HAL_DCU_TRACE)

/* Declared unused-tolerant because not every includer calls it: atomiq110's
 * nsx_platform_debug_init() delegates the DCU unlock to the BSP. */
static uint32_t nsx_platform_dcu_unlock_swo(void) __attribute__((unused));
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

    if (NSX_PWRCTRL_PWRSTOTP == 0) {
        bOffOtpOnExit = true;
        am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_OTP);
    }

    if (NSX_PWRCTRL_PWRSTCRYPTO == 0) {
        bOffCryptoOnExit = true;
        am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_CRYPTO);
    }

    if ((NSX_PWRCTRL_PWRSTCRYPTO == 1) &&
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
#if defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    if (mode == NSX_PERF_HIGH) {
        return am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE2);
    } else if (mode == NSX_PERF_MEDIUM) {
        return am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE1);
    } else {
        return am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER);
    }
#else
    if (mode == NSX_PERF_HIGH || mode == NSX_PERF_MEDIUM) {
        return am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE);
    } else {
        return am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER);
    }
#endif
}
