//*****************************************************************************
//
//! @file am_devices_display_generic_atomiq.c
//!
//! @brief Atomiq11x implementation of the generic display API.
//!
//! Uses am_hal_dc + am_devices_display_rm69330 instead of the Apollo4/5
//! nemadc_configure / DSI HAL path in am_devices_display_generic.c.
//!
//! Included from am_devices_display_generic.c when AM_PART_ATOMIQ11X_API is set.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include "am_hal_dsi.h"

//*****************************************************************************
//
//! Local types / state (mirrors the Apollo path in am_devices_display_generic.c)
//
//*****************************************************************************
typedef struct
{
    uint16_t ui16ResX;
    uint16_t ui16ResY;
    uint16_t ui16MinX;
    uint16_t ui16MinY;
    am_devices_disp_color_e eColorMode;
} am_devices_display_user_setting_t;

typedef struct
{
    volatile bool bXferPending;
    volatile bool bXferBusy;
    uint16_t ui16XferResX;
    uint16_t ui16XferResY;
    uint32_t ui32XferAddress;
    am_devices_disp_handler_t fnXferDoneCb;
    void *pArgXferDone;
    uint32_t total_stripe;
} am_devices_display_tranfer_t;

static am_devices_display_user_setting_t sDispUserSetting = {0};

static am_devices_display_tranfer_t sDispTransfer =
{
    .bXferPending = false,
    .bXferBusy = false,
    .ui16XferResX = 0,
    .ui16XferResY = 0,
    .ui32XferAddress = 0,
    .fnXferDoneCb = NULL,
    .pArgXferDone = NULL,
};

static void *g_pDCHandle = NULL;
static void *g_pDSIHandle = NULL;
static void *g_pDispHandle = NULL;

static am_hal_dc_config_t sDCCfg;
static am_hal_dsi_config_t sDSICfg;
static am_devices_dc_config_t sDevCfg;

static am_hal_dc_layer_t sDefaultDCLayer =
{
    .startx     = 0,
    .sizex      = 0,
    .resx       = 0,
    .starty     = 0,
    .sizey      = 0,
    .resy       = 0,
    .stride     = -1,
    .format     = AM_HAL_DC_LAYER_FORMAT_RGB24,
    .blendmode  = AM_HAL_DC_BLEND_SRC,
    .buscfg     = 0,
    .alpha      = 0xff,
    .flipx_en   = 0,
    .flipy_en   = 0,
    .extra_bits = 0
};

#ifdef ATOMIQ11X_FPGA
#define XFER_MAX_WAIT_MS                (2600 / (ATOMIQ11X_FPGA))
#else
#define XFER_MAX_WAIT_MS                100
#endif

#ifdef BAREMETAL
static bool bXferDone = false;
#else
static SemaphoreHandle_t semXferDone = NULL;
#endif

const IRQn_Type g_sInterrupts[] =
{
    GPIO0_001F_IRQn,
    GPIO0_203F_IRQn,
    GPIO0_405F_IRQn,
    GPIO0_607F_IRQn
};

#if defined(AM_BSP_GPIO_TOUCH_INT)
#if (TE_GPIO_IDX == TP_GPIO_IDX)
#if TE_GPIO_IDX == 0
#define disp_te_tp_isr        am_gpio0_001f_isr
#elif TE_GPIO_IDX == 1
#define disp_te_tp_isr        am_gpio0_203f_isr
#elif TE_GPIO_IDX == 2
#define disp_te_tp_isr        am_gpio0_405f_isr
#elif TE_GPIO_IDX == 3
#define disp_te_tp_isr        am_gpio0_607f_isr
#endif
#else
#if TE_GPIO_IDX == 0
#define disp_te_isr           am_gpio0_001f_isr
#elif TE_GPIO_IDX == 1
#define disp_te_isr           am_gpio0_203f_isr
#elif TE_GPIO_IDX == 2
#define disp_te_isr           am_gpio0_405f_isr
#elif TE_GPIO_IDX == 3
#define disp_te_isr           am_gpio0_607f_isr
#endif
#endif
#else
#if TE_GPIO_IDX == 0
#define disp_te_isr           am_gpio0_001f_isr
#elif TE_GPIO_IDX == 1
#define disp_te_isr           am_gpio0_203f_isr
#elif TE_GPIO_IDX == 2
#define disp_te_isr           am_gpio0_405f_isr
#elif TE_GPIO_IDX == 3
#define disp_te_isr           am_gpio0_607f_isr
#endif
#endif

static uint32_t am_devices_display_launch_transfer(void);
static uint32_t am_devices_display_prepare_transfer(void);

//*****************************************************************************
//
// GPIO TE ISR helpers
//
//*****************************************************************************
#if defined(AM_BSP_GPIO_TOUCH_INT)
#if (TE_GPIO_IDX == TP_GPIO_IDX)
void
disp_te_tp_isr(void)
{
    uint32_t ui32Status;
    am_hal_gpio_interrupt_irq_status_get(g_sInterrupts[TE_TP_GPIO_IDX], false, &ui32Status);
    am_hal_gpio_interrupt_irq_clear(g_sInterrupts[TE_TP_GPIO_IDX], ui32Status);
    am_hal_gpio_interrupt_service(g_sInterrupts[TE_TP_GPIO_IDX], ui32Status);
}
#else
void
disp_te_isr(void)
{
    uint32_t ui32Status;
    am_hal_gpio_interrupt_irq_status_get(g_sInterrupts[TE_GPIO_IDX], false, &ui32Status);
    am_hal_gpio_interrupt_irq_clear(g_sInterrupts[TE_GPIO_IDX], ui32Status);
    am_hal_gpio_interrupt_service(g_sInterrupts[TE_GPIO_IDX], ui32Status);
}
#endif
#else
void
disp_te_isr(void)
{
    uint32_t ui32Status;
    am_hal_gpio_interrupt_irq_status_get(g_sInterrupts[TE_GPIO_IDX], false, &ui32Status);
    am_hal_gpio_interrupt_irq_clear(g_sInterrupts[TE_GPIO_IDX], ui32Status);
    am_hal_gpio_interrupt_service(g_sInterrupts[TE_GPIO_IDX], ui32Status);
}
#endif

//*****************************************************************************
//
//! @brief DC ISR used by non-blocking frame transfers / TE.
//
//*****************************************************************************
void
am_disp_isr(void)
{
    uint32_t ui32Status;

    if (g_pDCHandle == NULL)
    {
        return;
    }

    am_hal_dc_interrupt_status_get(g_pDCHandle, &ui32Status, false);
    am_hal_dc_interrupt_clear(g_pDCHandle, ui32Status);
    am_hal_dc_interrupt_service(g_pDCHandle, ui32Status);
}

//*****************************************************************************
//
//! @brief TE callback compatible with am_hal_dc (single context arg).
//
//*****************************************************************************
static void
am_devices_display_te_handler(void *pvUnused)
{
    if (sDispTransfer.bXferPending)
    {
        am_devices_display_launch_transfer();
        sDispTransfer.bXferPending = false;
    }
}

//*****************************************************************************
//
//! @brief Frame-complete callback compatible with am_hal_dc.
//
//*****************************************************************************
static void
display_transfer_complete(void *pCallbackCtxt)
{
    sDispTransfer.bXferBusy = false;

    if (sDispTransfer.fnXferDoneCb)
    {
        if (sDispTransfer.total_stripe == 0)
        {
            sDispTransfer.fnXferDoneCb(sDispTransfer.pArgXferDone);
        }
        else
        {
            for (uint32_t i = 0; i < sDispTransfer.total_stripe; i++)
            {
                sDispTransfer.fnXferDoneCb((void *)i);
            }
        }
    }

#ifdef BAREMETAL
    bXferDone = true;
#else
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(semXferDone, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif
    (void)pCallbackCtxt;
}

//*****************************************************************************
//
// Recommended scanline helper
//
//*****************************************************************************
uint32_t
am_devices_display_set_scanline_recommended_parameter(uint8_t TETimesPerFrame)
{
    uint16_t ui16ScanLine = 0;

    if (TETimesPerFrame == 2)
    {
        ui16ScanLine = (sDispUserSetting.ui16MinY + 10) % g_sDispCfg.ui16ResY;
    }
    else if (TETimesPerFrame == 1)
    {
        ui16ScanLine = (sDispUserSetting.ui16MinY + g_sDispCfg.ui16ResY - 10) % g_sDispCfg.ui16ResY;
    }
    else
    {
        return AM_DEVICES_DISPLAY_STATUS_INVALID_ARG;
    }

    if (g_pDispHandle == NULL)
    {
        return AM_DEVICES_DISPLAY_STATUS_INVALID_HANDLE;
    }

    return am_devices_display_rm69330_set_scanline(g_pDispHandle, ui16ScanLine, g_sDispCfg.ui16ResY);
}

//*****************************************************************************
//
// Initialize display subsystem (DC + RM69330).
//
//*****************************************************************************
int32_t
am_devices_display_init(uint16_t ui16ResX,
                        uint16_t ui16ResY,
                        am_devices_disp_color_e eColorMode,
                        bool bEnableTE)
{
    uint32_t ui32Status;

    am_bsp_disp_pins_enable();

    sDispUserSetting.ui16ResX = ui16ResX;
    if (ui16ResX < g_sDispCfg.ui16ResX)
    {
        sDispUserSetting.ui16MinX = ((g_sDispCfg.ui16ResX - sDispUserSetting.ui16ResX) >> 2) << 1;
    }
    else
    {
        sDispUserSetting.ui16MinX = 0;
    }

    sDispUserSetting.ui16ResY = ui16ResY;
    if (ui16ResY < g_sDispCfg.ui16ResY)
    {
        sDispUserSetting.ui16MinY = ((g_sDispCfg.ui16ResY - sDispUserSetting.ui16ResY) >> 2) << 1;
    }
    else
    {
        sDispUserSetting.ui16MinY = 0;
    }

    sDispUserSetting.eColorMode = eColorMode;
    sDispTransfer.bXferPending = false;
    sDispTransfer.bXferBusy = false;

    if (!bEnableTE)
    {
        g_sDispCfg.eTEType = DISP_TE_DISABLE;
    }

    ui32Status = am_hal_dc_initialize(0, &g_pDCHandle);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_DISPLAY_STATUS_DPCTR_ERR;
    }

    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
        ui32Status = am_hal_dsi_initialize(0, &g_pDSIHandle);
        if (ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return AM_DEVICES_DISPLAY_STATUS_ERROR;
        }
    }

    ui32Status = am_hal_dc_power_control(g_pDCHandle, AM_HAL_SYSCTRL_WAKE, true);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_DISPLAY_STATUS_DPCTR_ERR;
    }

    //
    // Select LAU clock mux for DC (required on Atomiq FPGA bring-up path).
    //
    LAU->CTRL_b.CLKMUXSEL = 1;

    ui32Status = am_devices_display_rm69330_get_default_config(&sDCCfg,
                                                              (g_sDispCfg.eInterface == DISP_IF_DSI) ? &sDSICfg : NULL,
                                                              &sDevCfg);
    if (ui32Status != AM_DEVICES_DISPLAY_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    sDCCfg.sSPICfg.sTiming.resx = sDispUserSetting.ui16ResX;
    sDCCfg.sSPICfg.sTiming.resy = sDispUserSetting.ui16ResY;
    sDCCfg.bTEEnable = (g_sDispCfg.eTEType == DISP_TE_DC);

    if (eColorMode == COLOR_FORMAT_RGB565)
    {
        sDCCfg.ui32DstFormat = AM_HAL_DC_FORMAT_RGB565;
    }
    else
    {
        sDCCfg.ui32DstFormat = AM_HAL_DC_FORMAT_RGB888;
    }

    sDevCfg.ui16ResX = sDispUserSetting.ui16ResX;
    sDevCfg.ui16ResY = sDispUserSetting.ui16ResY;
    sDevCfg.ui16MinX = sDispUserSetting.ui16MinX + g_sDispCfg.ui16Offset;
    sDevCfg.ui16MinY = sDispUserSetting.ui16MinY;
    sDevCfg.bTEEnable = (g_sDispCfg.eTEType != DISP_TE_DISABLE);
    sDevCfg.ui32PixelFormat = sDCCfg.ui32DstFormat;

    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
        ui32Status = am_hal_dsi_power_control(g_pDSIHandle, AM_HAL_SYSCTRL_WAKE, true);
        if (ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return AM_DEVICES_DISPLAY_STATUS_ERROR;
        }
        ui32Status = am_hal_dsi_config(g_pDSIHandle, &sDSICfg);
        if (ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return AM_DEVICES_DISPLAY_STATUS_ERROR;
        }
    }

    ui32Status = am_hal_dc_config(g_pDCHandle, &sDCCfg);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_DISPLAY_STATUS_DPCTR_ERR;
    }

    am_devices_display_rm69330_hardware_reset();

    ui32Status = am_devices_display_rm69330_init(0, &sDevCfg, &g_pDispHandle, g_pDCHandle, g_pDSIHandle);
    if (ui32Status != AM_DEVICES_DISPLAY_STATUS_SUCCESS)
    {
        return AM_DEVICES_DISPLAY_STATUS_PANEL_ERR;
    }

#ifdef BAREMETAL
    bXferDone = false;
#else
    if (semXferDone == NULL)
    {
        semXferDone = xSemaphoreCreateBinary();
    }
#endif

    am_devices_display_set_scanline_recommended_parameter(1);

    if (g_sDispCfg.eTEType == DISP_TE_GPIO)
    {
        uint32_t IntNum = g_sDispCfg.ui16TEpin;
        am_hal_gpio_mask_t gpio_mask = AM_HAL_GPIO_MASK_DECLARE_ZERO;
        gpio_mask.U.Msk[GPIO_NUM2IDX(IntNum)] = GPIO_NUM2MSK(IntNum);
        am_hal_gpio_interrupt_clear(AM_HAL_GPIO_INT_CHANNEL_0, &gpio_mask);
        am_hal_gpio_interrupt_register(AM_HAL_GPIO_INT_CHANNEL_0,
                                       IntNum,
                                       (am_hal_gpio_handler_t)am_devices_display_te_handler,
                                       NULL);
        am_hal_gpio_interrupt_control(AM_HAL_GPIO_INT_CHANNEL_0,
                                      AM_HAL_GPIO_INT_CTRL_INDV_ENABLE,
                                      (void *)&IntNum);
        NVIC_SetPriority(g_sInterrupts[TE_GPIO_IDX], 0x4);
        NVIC_EnableIRQ(g_sInterrupts[TE_GPIO_IDX]);
    }

    if (g_sDispCfg.eTEType == DISP_TE_DC)
    {
        am_hal_dc_cb_t sTECb =
        {
            .pfnCallback = am_devices_display_te_handler,
            .pCallbackCtxt = NULL,
        };
        am_hal_dc_control(g_pDCHandle, AM_HAL_DC_REQ_SET_TE_ENABLE, &sTECb);
    }

    NVIC_SetPriority((IRQn_Type)DC_IRQn, 4);
    NVIC_EnableIRQ((IRQn_Type)DC_IRQn);

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Set display region
//
//*****************************************************************************
uint32_t
am_devices_display_set_region(uint16_t ui16ResX,
                              uint16_t ui16ResY,
                              uint16_t ui16MinX,
                              uint16_t ui16MinY)
{
    if (g_pDispHandle == NULL)
    {
        return AM_DEVICES_DISPLAY_STATUS_INVALID_HANDLE;
    }

    ui16MinX += g_sDispCfg.ui16Offset;

    sDCCfg.sSPICfg.sTiming.resx = ui16ResX;
    sDCCfg.sSPICfg.sTiming.resy = ui16ResY;
    am_hal_dc_config(g_pDCHandle, &sDCCfg);

    return am_devices_display_rm69330_set_region(g_pDispHandle,
                                                 ui16ResX,
                                                 ui16ResY,
                                                 ui16MinX,
                                                 ui16MinY);
}

//*****************************************************************************
//
// Flip display
//
//*****************************************************************************
uint32_t
am_devices_display_flip(uint8_t ui8FlipXY)
{
    if (g_pDispHandle == NULL)
    {
        return AM_DEVICES_DISPLAY_STATUS_INVALID_HANDLE;
    }

    return am_devices_display_rm69330_flip(g_pDispHandle, ui8FlipXY);
}

//*****************************************************************************
//
//! @brief Prepare layer for the pending frame transfer.
//
//*****************************************************************************
static uint32_t
am_devices_display_prepare_transfer(void)
{
    sDefaultDCLayer.sizex = sDefaultDCLayer.resx = sDispTransfer.ui16XferResX;
    sDefaultDCLayer.sizey = sDefaultDCLayer.resy = sDispTransfer.ui16XferResY;

    if (sDispUserSetting.eColorMode == COLOR_FORMAT_RGB565)
    {
        sDefaultDCLayer.format = AM_HAL_DC_LAYER_FORMAT_RGB565;
        sDefaultDCLayer.stride = 2 * sDispTransfer.ui16XferResX;
    }
    else
    {
        sDefaultDCLayer.format = AM_HAL_DC_LAYER_FORMAT_RGB24;
        sDefaultDCLayer.stride = 3 * sDispTransfer.ui16XferResX;
    }

    sDefaultDCLayer.baseaddr_virt = (void *)sDispTransfer.ui32XferAddress;
    sDefaultDCLayer.baseaddr_phys = (uintptr_t)sDispTransfer.ui32XferAddress;
    sDefaultDCLayer.blendmode = AM_HAL_DC_BLEND_SRC;
    sDefaultDCLayer.alpha = 0xff;

    if (am_hal_dc_layer_set(g_pDCHandle, 0, &sDefaultDCLayer) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_DISPLAY_STATUS_ERROR;
    }

    if (am_hal_dc_layer_enable(g_pDCHandle, 0) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_DISPLAY_STATUS_ERROR;
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Wait for transfer completion
//
//*****************************************************************************
uint32_t
am_devices_display_wait_transfer_done(void)
{
#ifdef BAREMETAL
    if (AM_HAL_STATUS_SUCCESS == am_hal_delay_us_status_change(XFER_MAX_WAIT_MS * 1000,
                                                               (uint32_t)&bXferDone,
                                                               0xFF,
                                                               true))
    {
        bXferDone = false;
    }
    else
    {
        return AM_DEVICES_DISPLAY_STATUS_ERROR;
    }
#else
    if (pdFALSE == xSemaphoreTake(semXferDone, XFER_MAX_WAIT_MS * portTICK_PERIOD_MS))
    {
        return AM_DEVICES_DISPLAY_STATUS_ERROR;
    }
#endif

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Launch frame transfer via DC nonblocking WRITE_MEMORY_START.
//
//*****************************************************************************
static uint32_t
am_devices_display_launch_transfer(void)
{
    am_hal_dc_transfer_t sTransaction =
    {
        .ui8Command = AM_HAL_DC_CMD_TYPE_WRITE_MEMORY_START,
    };

    uint32_t ui32Status = am_hal_dc_nonblocking_transfer(g_pDCHandle,
                                                         &sTransaction,
                                                         display_transfer_complete,
                                                         NULL);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        sDispTransfer.bXferBusy = false;
        return AM_DEVICES_DISPLAY_STATUS_ERROR;
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Common transfer path (TE-gated or immediate).
//
//*****************************************************************************
static uint32_t
am_devices_display_transfer_frame_inter(void)
{
    if (g_sDispCfg.eTEType != DISP_TE_DISABLE)
    {
        sDispTransfer.bXferPending = true;
    }

    if (am_devices_display_prepare_transfer() != AM_DEVICES_DISPLAY_STATUS_SUCCESS)
    {
        sDispTransfer.bXferBusy = false;
        return AM_DEVICES_DISPLAY_STATUS_ERROR;
    }

    if (g_sDispCfg.eTEType == DISP_TE_DISABLE)
    {
        return am_devices_display_launch_transfer();
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Send one frame via DC with a single layer
//
//*****************************************************************************
uint32_t
am_devices_display_transfer_frame(uint16_t ui16ResX,
                                  uint16_t ui16ResY,
                                  uint32_t ui32Address,
                                  am_devices_disp_handler_t fnXferDoneCb,
                                  void *pArgXferDone)
{
    if (sDispTransfer.bXferBusy)
    {
        return AM_DEVICES_DISPLAY_STATUS_TRY_AGAIN;
    }

    sDispTransfer.bXferBusy = true;
    sDispTransfer.bXferPending = false;
    sDispTransfer.ui16XferResX = ui16ResX;
    sDispTransfer.ui16XferResY = ui16ResY;
    sDispTransfer.ui32XferAddress = ui32Address;
    sDispTransfer.fnXferDoneCb = fnXferDoneCb;
    sDispTransfer.pArgXferDone = pArgXferDone;
    sDispTransfer.total_stripe = 0;

    return am_devices_display_transfer_frame_inter();
}

//*****************************************************************************
//
// Transfer one frame stripe by stripe
//
//*****************************************************************************
uint32_t
am_devices_display_transfer_frame_by_stripe(uint16_t ui16ResX,
                                            uint16_t ui16ResY,
                                            uint32_t ui32Address,
                                            uint32_t total_stripe,
                                            am_devices_disp_handler_t fnStripeDoneCb)
{
    if (sDispTransfer.bXferBusy)
    {
        return AM_DEVICES_DISPLAY_STATUS_TRY_AGAIN;
    }

    sDispTransfer.bXferBusy = true;
    sDispTransfer.bXferPending = false;
    sDispTransfer.ui16XferResX = ui16ResX;
    sDispTransfer.ui16XferResY = ui16ResY;
    sDispTransfer.ui32XferAddress = ui32Address;
    sDispTransfer.fnXferDoneCb = fnStripeDoneCb;
    sDispTransfer.pArgXferDone = NULL;
    sDispTransfer.total_stripe = total_stripe;

    return am_devices_display_transfer_frame_inter();
}

//*****************************************************************************
//
// De-initialize display subsystem
//
//*****************************************************************************
int32_t
am_devices_display_generic_deinit(void)
{
    NVIC_DisableIRQ(DC_IRQn);

    if (g_pDCHandle != NULL)
    {
        am_hal_dc_power_control(g_pDCHandle, AM_HAL_SYSCTRL_DEEPSLEEP, true);
        am_hal_dc_uninitialize(g_pDCHandle);
        g_pDCHandle = NULL;
    }

    if (g_pDSIHandle != NULL)
    {
        am_hal_dsi_power_control(g_pDSIHandle, AM_HAL_SYSCTRL_DEEPSLEEP, true);
        am_hal_dsi_uninitialize(g_pDSIHandle);
        g_pDSIHandle = NULL;
    }

    g_pDispHandle = NULL;
    am_bsp_disp_pins_disable();

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}
