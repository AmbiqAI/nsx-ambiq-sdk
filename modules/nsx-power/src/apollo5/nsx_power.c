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
// This source is staged for AmbiqSuite R5-backed NSX targets.
//
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
#if defined(AM_PART_APOLLO510L)
    if (mode == NSX_POWER_PERF_MAX) {
        retval = am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE2);
    } else if (mode == NSX_POWER_PERF_HIGH) {
        retval = am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE1);
    } else {
        retval = am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER);
    }
#else
    if ((mode == NSX_POWER_PERF_HIGH) || (mode == NSX_POWER_PERF_MAX)) {
        retval = am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE);
    } else {
        retval = am_hal_pwrctrl_mcu_mode_select(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER);
    }
#endif
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
    // Disable the XTAL.
    //
    if (!pCfg->need_xtal) {
        am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_RTC_SEL_LFRC, 0);
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

    if (!pCfg->need_audadc) {
        nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_AUDADC);
    }

}

void nsx_power_platform_memory_config(const nsx_power_config_t *pCfg) {
    // configure SRAM & other memories
    am_hal_pwrctrl_mcu_memory_config_t McuMemCfg =
    {
        .eROMMode       = AM_HAL_PWRCTRL_ROM_AUTO,

        #if defined(AM_PART_APOLLO5A)
            .bEnableCache   = true,
            .bRetainCache   = true,
            .eDTCMCfg       = pCfg->small_tcm ? AM_HAL_PWRCTRL_ITCM32K_DTCM128K : AM_HAL_PWRCTRL_ITCM256K_DTCM512K,
            .eRetainDTCM    = pCfg->small_tcm ? AM_HAL_PWRCTRL_ITCM32K_DTCM128K : AM_HAL_PWRCTRL_ITCM256K_DTCM512K,
        #elif defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B)
            .eDTCMCfg       = pCfg->small_tcm ? AM_HAL_PWRCTRL_ITCM32K_DTCM128K : AM_HAL_PWRCTRL_ITCM256K_DTCM512K,
            .eRetainDTCM    = AM_HAL_PWRCTRL_MEMRETCFG_TCMPWDSLP_RETAIN,
        #endif
        #if defined(AM_PART_APOLLO5A)
                .bEnableNVM     = true,
        #else
            #if defined(AM_PART_APOLLO510L)
                .eNVMCfg        = AM_HAL_PWRCTRL_NVM,
            #else
                .eNVMCfg        = AM_HAL_PWRCTRL_NVM0_AND_NVM1,
            #endif
        #endif // 5A
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
            #if !defined(AM_PART_APOLLO510L)
            .eSRAMCfg = AM_HAL_PWRCTRL_SRAM_3M,
            #else
            .eSRAMCfg = AM_HAL_PWRCTRL_SRAM_1P75M,
            #endif
            // .eActiveWithMCU   = PWRCTRL_SSRAMPWREN_PWRENSSRAM_ALL,
            .eActiveWithMCU   = AM_HAL_PWRCTRL_SRAM_NONE,
            .eActiveWithGFX   = AM_HAL_PWRCTRL_SRAM_NONE,
            .eActiveWithDISP  = AM_HAL_PWRCTRL_SRAM_NONE,
            #if !defined(AM_PART_APOLLO510L)
            .eSRAMRetain = AM_HAL_PWRCTRL_SRAM_3M
            #else
            .eSRAMRetain = AM_HAL_PWRCTRL_SRAM_1P75M
            #endif
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
        am_hal_spotmgr_profile_t spotmgr_profile;
        spotmgr_profile.PROFILE = 0;
        spotmgr_profile.PROFILE_b.COLLAPSESTMANDSTMP = 1;
        am_hal_spotmgr_profile_set(&spotmgr_profile);
    }
    #define ELP_ON                              1
    am_hal_pwrctrl_pwrmodctl_cpdlp_t sDefaultCpdlpConfig =
    {
         .eRlpConfig = AM_HAL_PWRCTRL_RLP_ON,
         #if ELP_ON
            .eElpConfig = AM_HAL_PWRCTRL_ELP_ON,
         #else
            .eElpConfig = AM_HAL_PWRCTRL_ELP_RET,
         #endif
         .eClpConfig = AM_HAL_PWRCTRL_CLP_ON
    };

    // Configure the cache power domain (CPDLP must precede cache enable).
    am_hal_pwrctrl_pwrmodctl_cpdlp_config(sDefaultCpdlpConfig);

    // Enable the I-Cache and D-Cache.
    am_hal_cachectrl_icache_enable();
    am_hal_cachectrl_dcache_enable(true);


    // am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_DIS_PERIPHS_ALL, 0);
    // MCUCTRL->XTALCTRL = 0;
    // am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_XTAL_PWDN_DEEPSLEEP, 0);
    if (!pCfg->need_xtal) {
        am_hal_rtc_osc_select(AM_HAL_RTC_OSC_LFRC); // Use LFRC instead of XT
        am_hal_rtc_osc_disable();
    }
    VCOMP->PWDKEY = VCOMP_PWDKEY_PWDKEY_Key;
    if (!pCfg->need_itm) {
        MCUCTRL->DBGCTRL = 0;
    }
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

    #if defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    //
    // Power off the RSS
    //
    am_hal_pwrctrl_rss_pwroff();
    #endif

    // Configure power mode
    nsx_delay_us(10000);
    NSX_TRY(nsx_power_set_performance_mode(pCfg->perf_mode), "Set CPU Perf mode failed.");
    MCUCTRL->MRAMCRYPTOPWRCTRL_b.CRYPTOCLKGATEN = 1;

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
    // Smallest TCM: 32K ITCM + 128K DTCM.  Single NVM bank.
    am_hal_pwrctrl_mcu_memory_config_t mem = {
        .eROMMode              = AM_HAL_PWRCTRL_ROM_AUTO,
        .eDTCMCfg              = AM_HAL_PWRCTRL_ITCM32K_DTCM128K,
        .eRetainDTCM           = AM_HAL_PWRCTRL_MEMRETCFG_TCMPWDSLP_RETAIN,
        .eNVMCfg               = AM_HAL_PWRCTRL_NVM0_ONLY,
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

    // MRAM low-power read mode + crypto clock gate
    MCUCTRL->MRAMCRYPTOPWRCTRL_b.MRAM0PWRCTRL    = 1;
    MCUCTRL->MRAMCRYPTOPWRCTRL_b.MRAM0LPREN      = 1;
    MCUCTRL->MRAMCRYPTOPWRCTRL_b.MRAM0SLPEN      = 0;
    MCUCTRL->MRAMCRYPTOPWRCTRL_b.CRYPTOCLKGATEN   = 1;

    return AM_HAL_STATUS_SUCCESS;
}

void nsx_power_disable_nvm(void) {
    PWRCTRL->MEMPWREN_b.PWRENNVM  = 0;
    PWRCTRL->MEMPWREN_b.PWRENNVM1 = 0;
    __DSB();
    __ISB();
}

void nsx_power_disable_caches(void) {
    SCB->ICIALLU = 0;          // Invalidate entire I-cache
    __DSB();
    __ISB();
    SCB->CCR &= ~SCB_CCR_IC_Msk;   // Disable I-cache
    SCB->CCR &= ~SCB_CCR_DC_Msk;   // Disable D-cache
    __DSB();
    __ISB();
}

void nsx_power_disable_debug(void) {
    MCUCTRL->DBGCTRL = 0;
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
