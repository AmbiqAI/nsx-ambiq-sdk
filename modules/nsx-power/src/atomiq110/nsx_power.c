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
#include "nsx_power.h"
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
static void nsx_power_disable_periph(am_hal_pwrctrl_periph_e peripheral) {
    (void)am_hal_pwrctrl_periph_disable(peripheral);
}

void nsx_power_platform_shutdown_peripherals(const nsx_power_config_t *pCfg) {

    //
    // Switch the RTC off the XTAL to LFRC. Despite the name, this does not
    // power down the XTAL itself (confirmed via disassembly) — only the
    // RTC's clock source changes.
    //
    if (!pCfg->need_xtal) {
        am_hal_rtc_osc_select(AM_HAL_RTC_OSC_LFRC);
        am_hal_rtc_osc_disable();
    }

    //
    // Turn off the voltage comparator.
    //
    VCOMP->PWDKEY = _VAL2FLD(VCOMP_PWDKEY_PWDKEY, VCOMP_PWDKEY_PWDKEY_Key);

    if (!pCfg->need_usb) {
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_USB);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_USBPHY);
    }

    if (!pCfg->need_iom) {
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM0);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM1);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM2);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM3);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM4);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM5);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM6);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM7);
    }

    if (!pCfg->need_uart) {
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_UART0);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_UART1);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_UART2);
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_UART3);
    }

    //
    // NOTE: Atomiq110 has no dedicated AUDADC peripheral (only LPADC), so
    // there is nothing to shut down here for pCfg->need_audadc.
    //

}

void nsx_power_platform_memory_config(const nsx_power_config_t *pCfg) {
    // configure SRAM & other memories
    am_hal_pwrctrl_mcu_memory_config_t McuMemCfg =
    {
        .eROMMode       = AM_HAL_PWRCTRL_ROM_AUTO,
        .eDTCMCfg       = pCfg->small_tcm ? AM_HAL_PWRCTRL_DTCM128K : AM_HAL_PWRCTRL_DTCM512K,
        .eRetainDTCM    = AM_HAL_PWRCTRL_MEMRETCFG_TCMPWDSLP_RETAIN,
        .eNVMCfg        = AM_HAL_PWRCTRL_NVM,
        .bKeepNVMOnInDeepSleep     = false
    };

    am_hal_pwrctrl_mcu_memory_config(&McuMemCfg);

    if (pCfg->need_ssram == false) {
        am_hal_pwrctrl_sram_memcfg_t SRAMMemCfg = {
            .eSRAMCfg = AM_HAL_PWRCTRL_SRAM_NONE,
            .eActiveWithMCU  = AM_HAL_PWRCTRL_SRAM_NONE,
            .eActiveWithGFX  = AM_HAL_PWRCTRL_SRAM_NONE,
            .eActiveWithDISP = AM_HAL_PWRCTRL_SRAM_NONE,
            .eSRAMRetain     = AM_HAL_PWRCTRL_SRAM_NONE
        };
        am_hal_pwrctrl_sram_config(&SRAMMemCfg);

    } else {
         am_hal_pwrctrl_sram_memcfg_t SRAMMemCfg = {
            .eSRAMCfg = AM_HAL_PWRCTRL_SRAM_3M,
            .eActiveWithMCU   = AM_HAL_PWRCTRL_SRAM_NONE,
            .eActiveWithGFX   = AM_HAL_PWRCTRL_SRAM_NONE,
            .eActiveWithDISP  = AM_HAL_PWRCTRL_SRAM_NONE,
            .eSRAMRetain = AM_HAL_PWRCTRL_SRAM_3M
        };
        am_hal_pwrctrl_sram_config(&SRAMMemCfg);
    };
}


int32_t nsx_power_platform_config(const nsx_power_config_t *pCfg) {
    uint32_t ui32ReturnStatus = AM_HAL_STATUS_SUCCESS;

#ifndef NSX_DISABLE_API_VALIDATION
    if (pCfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(
            pCfg->api, &nsx_power_oldest_supported_version, &nsx_power_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

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
    am_hal_pwrctrl_pwrmodctl_cpdlp_config(sDefaultCpdlpConfig);

    // Enable the I-Cache and D-Cache.
    am_hal_cachectrl_icache_enable();
    am_hal_cachectrl_dcache_enable(true);

    // NOTE: the RTC-osc/VCOMP shutdown sequence that used to live here is
    // performed by nsx_power_platform_shutdown_peripherals() below (#53).
    // NOTE: Atomiq110's MCUCTRL has no DBGCTRL register; disabling the
    // DEBUG power domain below is sufficient here.
    // Powering down various peripheral power domains
    if (!pCfg->need_itm) {
        am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_DEBUG);
    }
    if (!pCfg->need_crypto) {
        am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_CRYPTO);
    }
    am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_OTP);

    nsx_power_platform_memory_config(pCfg);

    am_hal_sysctrl_fpu_enable();
    am_hal_sysctrl_fpu_stacking_enable(true);

    nsx_power_platform_shutdown_peripherals(pCfg);

    //
    // TODO(#53): unexplained 10 ms busy-wait inherited from FPGA bring-up,
    // paid on every boot. Suspected to mask power-domain settling after the
    // peripheral shutdowns above. Verify on atomiq110 FPGA whether it can be
    // deleted or replaced with a status poll, then close
    // https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/53.
    //
    nsx_delay_us(10000);

    // Configure power mode
    NSX_TRY(nsx_power_set_performance_mode(pCfg->perf_mode), "Set CPU Perf mode failed.");

    if (pCfg->need_tempco) {
        nsx_printf("WARNING TempCo not supported.\n");
    }

    return ui32ReturnStatus;
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
    // Smallest DTCM, single NVM bank.
    am_hal_pwrctrl_mcu_memory_config_t mem = {
        .eROMMode              = AM_HAL_PWRCTRL_ROM_AUTO,
        .eDTCMCfg              = AM_HAL_PWRCTRL_DTCM128K,
        .eRetainDTCM           = AM_HAL_PWRCTRL_MEMRETCFG_TCMPWDSLP_RETAIN,
        .eNVMCfg               = AM_HAL_PWRCTRL_NVM,
        .bKeepNVMOnInDeepSleep = false,
    };
    am_hal_pwrctrl_mcu_memory_config(&mem);

    // No shared SRAM
    am_hal_pwrctrl_sram_memcfg_t sram = {
        .eSRAMCfg        = AM_HAL_PWRCTRL_SRAM_NONE,
        .eActiveWithMCU  = AM_HAL_PWRCTRL_SRAM_NONE,
        .eActiveWithGFX  = AM_HAL_PWRCTRL_SRAM_NONE,
        .eActiveWithDISP = AM_HAL_PWRCTRL_SRAM_NONE,
        .eSRAMRetain     = AM_HAL_PWRCTRL_SRAM_NONE,
    };
    am_hal_pwrctrl_sram_config(&sram);

    // NOTE: Atomiq110 has no MRAMCRYPTOPWRCTRL register, so there is no
    // MRAM low-power-read-mode / crypto-clock-gate poke to apply here
    // (unlike the Apollo5 backend).

    return AM_HAL_STATUS_SUCCESS;
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
    // NOTE: Atomiq110 has no MCUCTRL DBGCTRL register. Disabling the DEBUG
    // power domain is the equivalent step on this target.
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
