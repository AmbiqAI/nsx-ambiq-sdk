/**
 * @file nsx_pdm_apollo3.c
 * @brief PDM capture driver for Apollo3-family SoCs.
 */

#include "nsx_audio.h"

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "nsx_core.h"

#define NSX_PDM_MAX_INSTANCES 1

static nsx_audio_config_t *g_pdm_cfgs[NSX_PDM_MAX_INSTANCES] = {0};
static am_hal_pdm_transfer_t g_xfers[NSX_PDM_MAX_INSTANCES];
static const IRQn_Type g_pdm_irqs[] = {PDM_IRQn};

static void nsx_audio_pdm_pins_enable(uint32_t mic_idx) {
    if (mic_idx != 0) {
        return;
    }
#if defined(AM_BSP_GPIO_PDMCLK) && defined(AM_BSP_GPIO_PDM_DATA)
    am_hal_gpio_pinconfig(AM_BSP_GPIO_PDMCLK, g_AM_BSP_GPIO_PDMCLK);
    am_hal_gpio_pinconfig(AM_BSP_GPIO_PDM_DATA, g_AM_BSP_GPIO_PDM_DATA);
#elif defined(AM_BSP_GPIO_DMIC0_PDMC) && defined(AM_BSP_GPIO_DMIC0_PDMD)
    am_hal_gpio_pinconfig(AM_BSP_GPIO_DMIC0_PDMC, g_AM_BSP_GPIO_DMIC0_PDMC);
    am_hal_gpio_pinconfig(AM_BSP_GPIO_DMIC0_PDMD, g_AM_BSP_GPIO_DMIC0_PDMD);
#endif
}

static void nsx_audio_pdm_pins_disable(uint32_t mic_idx) {
    if (mic_idx != 0) {
        return;
    }
#if defined(AM_BSP_GPIO_PDMCLK) && defined(AM_BSP_GPIO_PDM_DATA)
    am_hal_gpio_pinconfig(AM_BSP_GPIO_PDMCLK, g_AM_HAL_GPIO_DISABLE);
    am_hal_gpio_pinconfig(AM_BSP_GPIO_PDM_DATA, g_AM_HAL_GPIO_DISABLE);
#elif defined(AM_BSP_GPIO_DMIC0_PDMC) && defined(AM_BSP_GPIO_DMIC0_PDMD)
    am_hal_gpio_pinconfig(AM_BSP_GPIO_DMIC0_PDMC, g_AM_HAL_GPIO_DISABLE);
    am_hal_gpio_pinconfig(AM_BSP_GPIO_DMIC0_PDMD, g_AM_HAL_GPIO_DISABLE);
#endif
}

static void pdm_isr_handler(uint32_t mic_idx) {
    if (mic_idx >= NSX_PDM_MAX_INSTANCES) {
        return;
    }
    nsx_audio_config_t *cfg = g_pdm_cfgs[mic_idx];
    if (cfg == NULL || cfg->_pdm_handle == NULL) {
        return;
    }

    am_hal_interupt_service_msg_t isr_data = {0};
    am_hal_pdm_interrupt_service(cfg->_pdm_handle, &isr_data, &g_xfers[mic_idx]);

    if (isr_data.isrStat & AM_HAL_PDM_INT_DCMP) {
        uint32_t *raw = (uint32_t *)g_xfers[mic_idx].ui32BufferWithDataAddr;
        if (raw == NULL) {
            raw = (uint32_t *)am_hal_pdm_dma_get_buffer(cfg->_pdm_handle);
        }

        uint32_t n = cfg->num_samples * cfg->num_channels;
        int16_t *pcm = cfg->pcm_buffer;
        for (uint32_t i = 0; i < n; i++) {
            pcm[i] = (int16_t)(raw[i] & 0xFFFF);
        }

        g_xfers[mic_idx].ui32BufferWithDataAddr = 0;
        if (cfg->callback != NULL) {
            cfg->callback(cfg, pcm, n);
        }
    }
}

void am_pdm0_isr(void) { pdm_isr_handler(0); }

const nsx_audio_pdm_config_t nsx_audio_pdm_default = {
    .clock        = NSX_AUDIO_CLK_HFRC,
    .clock_freq   = NSX_AUDIO_PDM_CLK_750KHZ,
    .mic          = NSX_AUDIO_PDM_MIC0,
    .sample_width = NSX_AUDIO_PDM_16BIT,
    .left_gain    = NSX_AUDIO_PDM_GAIN_0DB,
    .right_gain   = NSX_AUDIO_PDM_GAIN_0DB,
};

uint32_t nsx_audio_init(nsx_audio_config_t *cfg) {
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (cfg->source != NSX_AUDIO_SOURCE_PDM || cfg->callback == NULL) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (cfg->dma_buffer == NULL || cfg->pcm_buffer == NULL) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (((uintptr_t)cfg->dma_buffer & 0x1F) != 0) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (cfg->num_samples == 0 || (cfg->num_channels != 1 && cfg->num_channels != 2)) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    uint32_t mic_idx = (uint32_t)cfg->pdm.mic;
    if (mic_idx != 0) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (cfg->pdm.sample_width != NSX_AUDIO_PDM_16BIT) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (cfg->pdm.clock == NSX_AUDIO_CLK_PLL || cfg->pdm.clock == NSX_AUDIO_CLK_HFRC2_ADJ ||
        cfg->pdm.clock == NSX_AUDIO_CLK_HFXTAL) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    uint32_t samples_total = cfg->num_samples * cfg->num_channels;
    uint32_t dma_frame_bytes = samples_total * cfg->pdm.sample_width * 2;
    uint32_t pcm_frame_bytes = samples_total * sizeof(int16_t);
    if (cfg->dma_buffer_size < dma_frame_bytes * 2 || cfg->pcm_buffer_size < pcm_frame_bytes) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    am_hal_pdm_config_t pdm_hal = {
        .eClkDivider = AM_HAL_PDM_MCLKDIV_1,
        .eLeftGain = (am_hal_pdm_gain_e)cfg->pdm.left_gain,
        .eRightGain = (am_hal_pdm_gain_e)cfg->pdm.right_gain,
        .ui32DecimationRate = (cfg->pdm.clock_freq == NSX_AUDIO_PDM_CLK_750KHZ) ? 24 : 48,
        .bHighPassEnable = true,
        .ui32HighPassCutoff = 0x3,
        .ePDMClkSpeed = (cfg->pdm.clock_freq == NSX_AUDIO_PDM_CLK_750KHZ) ? AM_HAL_PDM_CLK_750KHZ : AM_HAL_PDM_CLK_1_5MHZ,
        .bInvertI2SBCLK = false,
        .ePDMClkSource = AM_HAL_PDM_INTERNAL_CLK,
        .bPDMSampleDelay = false,
        .bDataPacking = true,
        .ePCMChannels = (cfg->num_channels == 2) ? AM_HAL_PDM_CHANNEL_STEREO : AM_HAL_PDM_CHANNEL_LEFT,
        .ui32GainChangeDelay = 0,
        .bI2SEnable = false,
        .bSoftMute = false,
        .bLRSwap = false,
        .bDoNotStartPdm = false,
    };

    uint32_t rc = am_hal_pdm_initialize(mic_idx, &cfg->_pdm_handle);
    if (rc != AM_HAL_STATUS_SUCCESS) {
        return NSX_STATUS_INIT_FAILED;
    }
    rc = am_hal_pdm_power_control(cfg->_pdm_handle, AM_HAL_PDM_POWER_ON, false);
    if (rc != AM_HAL_STATUS_SUCCESS) {
        return NSX_STATUS_INIT_FAILED;
    }
    rc = am_hal_pdm_configure(cfg->_pdm_handle, &pdm_hal);
    if (rc != AM_HAL_STATUS_SUCCESS) {
        return NSX_STATUS_INIT_FAILED;
    }

    nsx_audio_pdm_pins_enable(mic_idx);
    am_hal_pdm_fifo_flush(cfg->_pdm_handle);
    am_hal_pdm_fifo_threshold_setup(cfg->_pdm_handle, AM_HAL_PDM_DMA_THRESHOLD_MIN);
    am_hal_pdm_interrupt_enable(cfg->_pdm_handle,
                                AM_HAL_PDM_INT_DERR | AM_HAL_PDM_INT_DCMP |
                                AM_HAL_PDM_INT_UNDFL | AM_HAL_PDM_INT_OVF);
    NVIC_SetPriority(g_pdm_irqs[mic_idx], 0x03);
    NVIC_ClearPendingIRQ(g_pdm_irqs[mic_idx]);
    NVIC_EnableIRQ(g_pdm_irqs[mic_idx]);

    g_pdm_cfgs[mic_idx] = cfg;
    cfg->_started = 0;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_audio_start(nsx_audio_config_t *cfg) {
    if (cfg == NULL || cfg->_pdm_handle == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    uint32_t mic_idx = (uint32_t)cfg->pdm.mic;
    uint32_t n = cfg->num_samples * cfg->num_channels;
    g_xfers[mic_idx].ui32TargetAddr = (uint32_t)(((uintptr_t)cfg->dma_buffer + 3) & ~0xFU);
    g_xfers[mic_idx].ui32TargetAddrReverse = g_xfers[mic_idx].ui32TargetAddr + n * cfg->pdm.sample_width * 2;
    g_xfers[mic_idx].ui32BufferWithDataAddr = 0;
    g_xfers[mic_idx].ui32TotalCount = n * cfg->pdm.sample_width * 2;
    g_xfers[mic_idx].bRestartDMA = true;

    am_hal_pdm_enable(cfg->_pdm_handle);
    am_hal_pdm_dma_start(cfg->_pdm_handle, &g_xfers[mic_idx]);
    cfg->_started = 1;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_audio_stop(nsx_audio_config_t *cfg) {
    if (cfg == NULL || cfg->_pdm_handle == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    uint32_t mic_idx = (uint32_t)cfg->pdm.mic;
    NVIC_DisableIRQ(g_pdm_irqs[mic_idx]);
    am_hal_pdm_interrupt_clear(cfg->_pdm_handle, 0xFFFFFFFF);
    am_hal_pdm_interrupt_disable(cfg->_pdm_handle, 0xFFFFFFFF);
    am_hal_pdm_disable(cfg->_pdm_handle);
    am_hal_pdm_power_control(cfg->_pdm_handle, AM_HAL_PDM_POWER_OFF, false);
    am_hal_pdm_deinitialize(cfg->_pdm_handle);
    nsx_audio_pdm_pins_disable(mic_idx);

    cfg->_pdm_handle = NULL;
    cfg->_started = 0;
    g_pdm_cfgs[mic_idx] = NULL;
    return NSX_STATUS_SUCCESS;
}
