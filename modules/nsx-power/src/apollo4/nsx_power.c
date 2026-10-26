/**
 * @brief Apollo4 power helpers
 */

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_power_internal.h"

#if !defined(AM_PART_APOLLO4L)
uint32_t nsx_power_tempco_enable(void);
void nsx_power_tempco_prepare_for_sleep(void);
#endif

static uint32_t nsx_power_disable_periph(am_hal_pwrctrl_periph_e peripheral) {
    return am_hal_pwrctrl_periph_disable(peripheral);
}

static uint32_t nsx_power_platform_shutdown_peripherals(const nsx_power_config_t *cfg) {
    NSX_POWER_RETURN_IF_ERROR(am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_RTC_SEL_LFRC, 0));
    VCOMP->PWDKEY = _VAL2FLD(VCOMP_PWDKEY_PWDKEY, VCOMP_PWDKEY_PWDKEY_Key);

#ifdef AM_DEVICES_BLECTRLR_RESET_PIN
    if (!cfg->need_ble) {
        NSX_POWER_RETURN_IF_ERROR(am_hal_gpio_state_write(AM_DEVICES_BLECTRLR_RESET_PIN, AM_HAL_GPIO_OUTPUT_CLEAR));
        NSX_POWER_RETURN_IF_ERROR(am_hal_gpio_pinconfig(AM_DEVICES_BLECTRLR_RESET_PIN, am_hal_gpio_pincfg_output));
        NSX_POWER_RETURN_IF_ERROR(am_hal_gpio_state_write(AM_DEVICES_BLECTRLR_RESET_PIN, AM_HAL_GPIO_OUTPUT_SET));
        NSX_POWER_RETURN_IF_ERROR(am_hal_gpio_state_write(AM_DEVICES_BLECTRLR_RESET_PIN, AM_HAL_GPIO_OUTPUT_CLEAR));
    }
#endif

    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOS));
    if (!cfg->need_iom) {
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM0));
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM1));
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM2));
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM3));
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM4));
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM5));
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM6));
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_IOM7));
    }
    if (!cfg->need_uart) {
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_UART0));
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_UART3));
    }

    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_UART1));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_UART2));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_ADC));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_MSPI0));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_MSPI1));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_MSPI2));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_GFX));
#ifndef AM_PART_APOLLO4L
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_DISP));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_DISPPHY));
    if (!cfg->need_usb) {
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_USB));
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_USBPHY));
    }
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_PDM1));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_PDM2));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_PDM3));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_I2S1));
#endif
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_SDIO));
    if (!cfg->need_itm) {
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_DEBUG));
    }
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_AUDREC));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_AUDPB));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_PDM0));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_I2S0));
#ifndef AM_PART_APOLLO4L
    if (!cfg->need_audadc) {
        NSX_POWER_RETURN_IF_ERROR(nsx_power_disable_periph(AM_HAL_PWRCTRL_PERIPH_AUDADC));
    }
#endif
    if (!cfg->need_crypto) {
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_CRYPTO_POWERDOWN, 0));
    }
    if (!cfg->need_xtal) {
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_control(AM_HAL_PWRCTRL_CONTROL_XTAL_PWDN_DEEPSLEEP, 0));
    }
    return NSX_STATUS_SUCCESS;
}

static uint32_t nsx_power_platform_memory_config(const nsx_power_config_t *cfg) {
    if (!cfg->need_ssram) {
        am_hal_pwrctrl_sram_memcfg_t sram_cfg = {
            .eSRAMCfg = AM_HAL_PWRCTRL_SRAM_NONE,
            .eActiveWithMCU = AM_HAL_PWRCTRL_SRAM_NONE,
            .eActiveWithDSP = AM_HAL_PWRCTRL_SRAM_NONE,
            .eSRAMRetain = AM_HAL_PWRCTRL_SRAM_NONE,
        };
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_sram_config(&sram_cfg));
#ifndef AM_PART_APOLLO4L
        am_hal_pwrctrl_dsp_memory_config_t ext_sram_cfg = {
            .bEnableICache = false,
            .bRetainCache = false,
            .bEnableRAM = false,
            .bActiveRAM = false,
            .bRetainRAM = false,
        };
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_dsp_memory_config(AM_HAL_DSP0, &ext_sram_cfg));
#endif
        return NSX_STATUS_SUCCESS;
    }

    am_hal_daxi_config_t daxi_cfg = {
        .bDaxiPassThrough = false,
        .bAgingSEnabled = false,
        .eAgingCounter = AM_HAL_DAXI_CONFIG_AGING_1024,
        .eNumBuf = AM_HAL_DAXI_CONFIG_NUMBUF_32,
        .eNumFreeBuf = AM_HAL_DAXI_CONFIG_NUMFREEBUF_3,
    };
    NSX_POWER_RETURN_IF_ERROR(am_hal_daxi_config(&daxi_cfg));

    if (cfg->small_tcm) {
        am_hal_pwrctrl_mcu_memory_config_t mcu_mem_cfg = {
            .eCacheCfg = AM_HAL_PWRCTRL_CACHE_ALL,
            .bRetainCache = true,
            .eDTCMCfg = AM_HAL_PWRCTRL_DTCM_128K,
            .eRetainDTCM = AM_HAL_PWRCTRL_DTCM_128K,
            .bEnableNVM0 = true,
            .bRetainNVM0 = false,
        };
        NSX_POWER_RETURN_IF_ERROR(am_hal_pwrctrl_mcu_memory_config(&mcu_mem_cfg));
    }
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_power_platform_validate_config(const nsx_power_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (cfg->api == NULL || nsx_core_check_api(cfg->api, &nsx_power_oldest_supported_version, &nsx_power_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    if (cfg->perf_mode != NSX_POWER_PERF_LOW && cfg->perf_mode != NSX_POWER_PERF_HIGH &&
        cfg->perf_mode != NSX_POWER_PERF_MAX) {
        return NSX_STATUS_INVALID_CONFIG;
    }
#if defined(AM_PART_APOLLO4L)
    if (cfg->need_tempco) {
        return NSX_STATUS_INVALID_CONFIG;
    }
#endif

    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_power_platform_config(const nsx_power_config_t *cfg) {
    NSX_POWER_RETURN_IF_ERROR(nsx_power_platform_validate_config(cfg));
    am_bsp_low_power_init();
    NSX_POWER_RETURN_IF_ERROR(am_hal_cachectrl_config(&am_hal_cachectrl_defaults));
    NSX_POWER_RETURN_IF_ERROR(am_hal_cachectrl_enable());
    NSX_POWER_RETURN_IF_ERROR(nsx_power_platform_memory_config(cfg));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_platform_shutdown_peripherals(cfg));
    NSX_POWER_RETURN_IF_ERROR(nsx_power_set_performance_mode(cfg->perf_mode));

#if !defined(AM_PART_APOLLO4L)
    if (cfg->need_tempco) {
        NSX_POWER_RETURN_IF_ERROR(nsx_power_tempco_enable());
    }
#endif

    return AM_HAL_STATUS_SUCCESS;
}

void nsx_power_platform_deep_sleep(void) {
#if !defined(AM_PART_APOLLO4L)
    nsx_power_tempco_prepare_for_sleep();
#endif
    am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
}
