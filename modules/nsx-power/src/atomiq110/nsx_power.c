/**
 * @brief Power Control Utilities
 *
 *
 */

//*****************************************************************************
//
// Copyright (c) Ambiq Micro, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This source is staged for AmbiqSuite Atomiq110-backed NSX targets.
//
// NOTE: This backend is adapted from the Apollo5 nsx-power implementation.
// Atomiq110's HAL differs from Apollo5 in several ways relevant here:
//   - There is no MRAMCRYPTOPWRCTRL register (no MRAM/RRAM crypto
//     power-gating block), so those pokes are simply omitted.
//   - NVM config is a single-bank enum (AM_HAL_PWRCTRL_NVM /
//     AM_HAL_PWRCTRL_NVM_NONE) instead of Apollo5's NVM0/NVM1 dual-bank
//     enum, and PWRCTRL->MEMPWREN has no PWRENNVM1 field.
//   - There is no separate AUDADC peripheral (only LPADC), so the AUDADC
//     shutdown branch is omitted rather than mapped to LPADC.
//   - SpotManager uses a stimulus/state-driven API
//     (am_hal_spotmgr_power_state_update()) rather than Apollo5's
//     profile-struct API, and does not expose an STM/STMP "collapse"
//     concept, so pCfg->spotmgr_collapse is a no-op here (see below).
//*****************************************************************************

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_debug_internal.h"
#include "nsx_power_internal.h"
#include "am_hal_clkmgr.h"
#include "am_hal_spotmgr.h"

uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode) {
    uint32_t retval = NSX_STATUS_SUCCESS;
    if ((mode == NSX_POWER_PERF_HIGH) || (mode == NSX_POWER_PERF_MAX)) {
        retval = am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE);
    } else {
        retval = am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER);
    }
    return retval;
}

//*****************************************************************************
//
// Internal method for turning off peripherals
//
//*****************************************************************************
static uint32_t nsx_power_platform_shutdown_peripherals(const nsx_power_config_t *pCfg) {
    //
    // Switch the RTC off the XTAL to LFRC. Despite the name, this does not
    // power down the XTAL itself (confirmed via disassembly) — only the
    // RTC's clock source changes.
    //
    if (!pCfg->need_xtal) {
        am_hal_rtc_osc_select(AM_HAL_RTC_OSC_LFRC);
        NSX_POWER_RETURN_IF_ERROR(am_hal_rtc_osc_disable());
    }

    //
    // Turn off the voltage comparator.
    //
    VCOMP->PWDKEY = _VAL2FLD(VCOMP_PWDKEY_PWDKEY, VCOMP_PWDKEY_PWDKEY_Key);

    if (!pCfg->need_usb) {
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_USB));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_USBPHY));
    }

    if (!pCfg->need_iom) {
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM0));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM1));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM2));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM3));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM4));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM5));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM6));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM7));
    }

    if (!pCfg->need_uart) {
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_UART0));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_UART1));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_UART2));
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_UART3));
    }

    //
    // NOTE: Atomiq110 has no dedicated AUDADC peripheral (only LPADC), so
    // there is nothing to shut down here for pCfg->need_audadc.
    //
    return NSX_STATUS_SUCCESS;
}

static uint32_t configure_memory(am_hal_pwrctrl_dtcm_select_e dtcm, am_hal_pwrctrl_sram_select_e ssram) {
    am_hal_pwrctrl_mcu_memory_config_t mem = {
        .eROMMode    = AM_HAL_PWRCTRL_ROM_AUTO,
        .eDTCMCfg    = dtcm,
        .eRetainDTCM = AM_HAL_PWRCTRL_MEMRETCFG_TCMPWDSLP_RETAIN,
        .eNVMCfg     = AM_HAL_PWRCTRL_NVM,
    };
    NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_mcu_memory_config(&mem));
    return am_hal_pwrctrl_sram_config(ssram);
}

uint32_t nsx_power_platform_config(const nsx_power_config_t *pCfg) {
    am_bsp_low_power_init();

    if (pCfg->spotmgr_collapse) {
        // Atomiq110's SpotManager is stimulus/state driven
        // (am_hal_spotmgr_power_state_update()) and has no STM/STMP
        // "collapse" profile concept equivalent to Apollo5's
        // am_hal_spotmgr_profile_t.PROFILE_b.COLLAPSESTMANDSTMP. This is
        // intentionally a no-op on this target.
    }
    am_hal_pwrctrl_pwrmodctl_cpdlp_t sDefaultCpdlpConfig =
    {
         .eRlpConfig = AM_HAL_PWRCTRL_RLP_ON,
         .eElpConfig = AM_HAL_PWRCTRL_ELP_ON,
         .eClpConfig = AM_HAL_PWRCTRL_CLP_ON
    };

    // Configure the cache power domain (CPDLP must precede cache enable).
    NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_pwrmodctl_cpdlp_config(sDefaultCpdlpConfig));

    // Enable the I-Cache and D-Cache.
    NSX_POWER_RETURN_IF_ERROR(am_hal_cachectrl_icache_enable());
    NSX_POWER_RETURN_IF_ERROR(am_hal_cachectrl_dcache_enable(true));

    // NOTE: the RTC-osc/VCOMP shutdown sequence that used to live here is
    // performed by nsx_power_platform_shutdown_peripherals() below (#53).
    // Powering down various peripheral power domains
    if (!pCfg->need_itm) {
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_DEBUG));
    }
    if (!pCfg->need_crypto) {
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_CRYPTO));
        // The AT110 HAL requires OTP while the crypto domain is powered.
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_OTP));
    }

    NSX_POWER_RETURN_IF_ERROR(configure_memory(AM_HAL_PWRCTRL_DTCM512K, AM_HAL_PWRCTRL_SRAM_3M));

    am_hal_sysctrl_fpu_enable();
    am_hal_sysctrl_fpu_stacking_enable(true);

    NSX_POWER_RETURN_IF_ERROR(nsx_power_platform_shutdown_peripherals(pCfg));

    // Keep the inherited startup delay pending cold-power qualification (#53).
    nsx_delay_us(10000);

    // Configure power mode
    NSX_POWER_RETURN_IF_ERROR(nsx_power_set_performance_mode(pCfg->perf_mode));

    if (pCfg->need_tempco) {
        nsx_printf("WARNING TempCo not supported.\n");
    }

    return NSX_STATUS_SUCCESS;
}

/**
 * @brief Wraps am_hal_sysctrl_sleep to enable and disable
 * systems as needed.
 *
 */
void nsx_power_platform_deep_sleep(void) {
    am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
}

/* ===================================================================
 * Power measurement helpers
 * =================================================================== */

uint32_t nsx_power_shutdown_peripherals(void) {
    uint32_t status = nsx_debug_before_hw_init();
    if (status != 0) return status;
    // Disable all device and audio-subsystem power domains
    am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_DIS_PERIPHS_ALL, 0);
    am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_XTAL_PWDN_DEEPSLEEP, 0);

    // Voltage comparator off
    VCOMP->PWDKEY = VCOMP_PWDKEY_PWDKEY_Key;

    // Stop all 16 hardware timers
    for (uint32_t t = 0; t < 16; t++) {
        am_hal_timer_stop(t);
    }

    return AM_HAL_STATUS_SUCCESS;
}

uint32_t nsx_power_minimize_memory(void) {
    // The caller supplies a constrained layout for this explicit measurement helper.
    return configure_memory(AM_HAL_PWRCTRL_DTCM128K, AM_HAL_PWRCTRL_SRAM_NONE);
}

void nsx_power_disable_nvm(void) {
    PWRCTRL->MEMPWREN_b.PWRENNVM = 0;
    __DSB();
    __ISB();
}

void nsx_power_disable_caches(void) {
    // Clean dirty D-cache lines before disabling it; the disable below
    // doesn't write them back on its own, and this part enables D-cache
    // by default.
    SCB_CleanInvalidateDCache();
    SCB->ICIALLU = 0;          // Invalidate entire I-cache
    __DSB();
    __ISB();
    SCB->CCR &= ~SCB_CCR_IC_Msk;   // Disable I-cache
    SCB->CCR &= ~SCB_CCR_DC_Msk;   // Disable D-cache
    __DSB();
    __ISB();
}

void nsx_power_disable_debug(void) {
    nsx_debug_disable(NSX_DEBUG_ITM);
    am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_DEBUG);
}

void nsx_power_tristate_gpios(const uint32_t *keep_pins, uint32_t n_keep) {
    for (uint32_t pin = 0; pin < AM_HAL_GPIO_MAX_PADS; pin++) {
        bool keep = false;
        for (uint32_t i = 0; i < n_keep; i++) {
            if (keep_pins[i] == pin) {
                keep = true;
                break;
            }
        }
        if (!keep) {
            am_hal_gpio_pinconfig(pin, am_hal_gpio_pincfg_disabled);
        }
    }
}
