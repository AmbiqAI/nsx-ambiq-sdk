//*****************************************************************************
//
//! @file am_devices_display_fl7703ni.c
//!
//! @brief Driver for Forcelead FL7703NI panel (MIPI-DSI video mode).
//!
//! Panel register bring-up uses the DSI APB packet interface and does not
//! require DC or DBIDSIBRIDGE. Continuous frame refresh uses DC DPI into the
//! DSI host video engine (configured by the application / test).
//!
//! @addtogroup display_fl7703ni FL7703NI Display Device Driver
//! @ingroup devices
//! @{
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

#include "am_devices_display_fl7703ni.h"
#include "am_util_delay.h"
#include "am_bsp.h"
#include "am_hal_dc.h"

//
// Panel command sequences require millisecond delays between steps.
//
#ifndef SIMULATION
#define DELAY am_util_delay_ms
#else
#define DELAY(...)
#endif

//! Per-instance driver state for FL7703NI.
typedef struct
{
    uint32_t                    ui32Module;   //!< Module index.
    void                        *pDCHandle;   //!< Display controller handle (DPI).
    void                        *pDSIHandle;  //!< DSI handle (APB packet cmds).
    bool                        bOccupied;    //!< True after successful init.
}
am_devices_fl7703ni_t;

am_devices_fl7703ni_t gFl7703ni[AM_REG_DC_NUM_MODULES];

//! Default DC DPI configuration (pixel path into DSI video mode).
am_hal_dc_config_t gsDCDPI =
{
    .eInterface = AM_HAL_DC_IF_DPI,
    .bTEEnable = false,
    //
    // DPI pixel packing: AM_HAL_DC_FORMAT_DPI_RGB888, AM_HAL_DC_FORMAT_DPI_RGB666_CFG1, AM_HAL_DC_FORMAT_DPI_RGB666_CFG2,
    // AM_HAL_DC_FORMAT_DPI_RGB565_CFG1, AM_HAL_DC_FORMAT_DPI_RGB565_CFG2, or AM_HAL_DC_FORMAT_DPI_RGB565_CFG3.
    //
    .ui32DstFormat = AM_HAL_DC_FORMAT_DPI_RGB888,
    //
    // Default 640x480 panel timing (1:1 mapping).
    //
    .sDPICfg.sTiming.resx = 640,
    .sDPICfg.sTiming.resy = 480,
    .sDPICfg.sTiming.fpx = 56,  //!< HFP
    .sDPICfg.sTiming.fpy = 10,  //!< VFP
    .sDPICfg.sTiming.bpx = 3,   //!< HBP
    .sDPICfg.sTiming.bpy = 8,   //!< VBP
    .sDPICfg.sTiming.blx = 4,   //!< HSA
    .sDPICfg.sTiming.bly = 2,   //!< VSA
#ifdef ATOMIQ11X_FPGA
    .sDPICfg.fTargetClkPClk = ATOMIQ11X_FPGA,
#else
    .sDPICfg.fTargetClkPClk = 96.0,
#endif
};

//! Default DSI video-mode host configuration.
am_hal_dsi_config_t gsDSI =
{
#ifdef ATOMIQ11X_FPGA
    .ui8LanesNum = 1,           //!< FPGA M31 interposer supports 1 lane only.
#else
    .ui8LanesNum = 2,
#endif
    .bContinuousClock = false,
    .ui8TXGAP = 0x4,
    .bAutoInsertEOTP = true,
#ifdef ATOMIQ11X_FPGA
    .bPPI16EN = false,          //!< FPGA M31 interposer supports 8-bit PPI only.
#else
    .bPPI16EN = true,
#endif
    .bVidEnable = true,
    .eVidPixelFormat = AM_HAL_DSI_VIDEO_PIXEL_FORMAT_RGB_565_16BIT,
    .eVidMode = AM_HAL_DSI_VIDEO_MODE_NON_BURST_SYNC_EVENTS,
    .ui16VidStartDelay = 1600,  //!< Delay so the pixel FIFO can fill.
    .eVidBllpMode = AM_HAL_DSI_VIDEO_BLLP_MODE_BLANKING_H_BLANKING_V,
    .bVidUseNullPktBllp = true,
    .ui8VidVC = 0,
    .bSSCEn = 1,
};

//
// M31 MIPI Tx PHY timing tables (index selected in get_default_config).
//
am_hal_dsi_pll_time_config_t sPllTimecfgs[] =
{
#if 0
    {
        // Refclk_in_sel = AM_HAL_DSI_25MHZ;
        // BitRate = 100Mbps;
        // SSCEn = 1;
        // SSCPRD = 30.0KMz;
        // SSCDelta = 2500.0ppm;
        .ui32DataLaneHS_b.ui32DataLaneHSPreTime = 0x11,
        .ui32DataLaneHS_b.ui32DataLaneHSZeroTime = 0x21,
        .ui32DataLaneHS_b.ui32DataLaneHSTrailTime = 0x24,
        .ui32DataLaneHS_b.ui32ClockLaneHSPreTime = 0x7,
        .ui32ClockLaneHS_b.ui32ClockLaneHSZeroTime = 0x36,
        .ui32ClockLaneHS_b.ui32ClockLaneHSTrailTime = 0x10,
        .ui32ClockLaneHS_b.ui32ClockLaneHSCLKPreTime = 0x10,
        .ui32ClockLaneHS_b.ui32ClockLaneHSCLKPostTime = 0x74,
        .ui32Time_b.ui32TexitTime = 0x14,
        .ui32Time_b.ui32HSTLPTime = 0xA,

        .ui32Time_b.ui32ExtdCycleSel = 0x4,
        .ui32PLLFBK_b.ui32PLLPreDiv = 0x1,
        .ui32PLLFBK_b.ui32PLLFBKInit = 0x80,
        .ui32PLLFBK_b.ui32PLLPosDiv = 0x1,
        .ui32PLLFBKFRA_b.ui32PLLFBKFRA = 0x0,
        .ui32PLLSSCDelta_b.ui32PLLSSCDelta = 0xC9A,
        .ui32PLL_b.ui32PLLSSCPRD = 0x68,
        .ui32BitRate = 100000000,
#ifdef ATOMIQ11X_FPGA
        .ui32PllPreDiv = 0x1,
        .ui32ExtdCycleSet = 0x4,
        .ui32PllFbkInt = 0x80,
        .ui32PllFbkFra = 0x0,
        .ui32DlaneHsPreTime = 0x10,
        .ui32DlaneHsZeroTime = 0x21,
        .ui32DlaneHsTrailTime = 0x17,
        .ui32ClaneHsPreTime = 0x7,
        .ui32ClaneHsZeroTime = 0x35,
        .ui32ClaneHsTrailTime = 0xF,
        .ui32ClaneHsClkPreTime = 0xF,
        .ui32ClaneHsClkPostTime = 0x73,
#endif
    },
#endif
    {
        // Refclk_in_sel = AM_HAL_DSI_25MHZ;
        // BitRate = 200Mbps;
        // SSCEn = 1;
        // SSCPRD = 30.0KMz;
        // SSCDelta = 2500.0ppm;
        .ui32DataLaneHS_b.ui32DataLaneHSPreTime = 0xD,
        .ui32DataLaneHS_b.ui32DataLaneHSZeroTime = 0x1B,
        .ui32DataLaneHS_b.ui32DataLaneHSTrailTime = 0x14,
        .ui32DataLaneHS_b.ui32ClockLaneHSPreTime = 0x7,
        .ui32ClockLaneHS_b.ui32ClockLaneHSZeroTime = 0x36,
        .ui32ClockLaneHS_b.ui32ClockLaneHSTrailTime = 0x10,
        .ui32ClockLaneHS_b.ui32ClockLaneHSCLKPreTime = 0x8,
        .ui32ClockLaneHS_b.ui32ClockLaneHSCLKPostTime = 0x40,
        .ui32Time_b.ui32TexitTime = 0x14,
        .ui32Time_b.ui32HSTLPTime = 0xA,

        .ui32Time_b.ui32ExtdCycleSel = 0x3,
        .ui32PLLFBK_b.ui32PLLPreDiv = 0x1,
        .ui32PLLFBK_b.ui32PLLFBKInit = 0x80,
        .ui32PLLFBK_b.ui32PLLPosDiv = 0x1,
        .ui32PLLFBKFRA_b.ui32PLLFBKFRA = 0x0,
        .ui32PLLSSCDelta_b.ui32PLLSSCDelta = 0xC9A,
        .ui32PLL_b.ui32PLLSSCPRD = 0x68,
        .ui32BitRate = 200000000,
#ifdef ATOMIQ11X_FPGA
        .ui32PllPreDiv = 0x1,
        .ui32ExtdCycleSet = 0x3,
        .ui32PllFbkInt = 0x80,
        .ui32PllFbkFra = 0x0,
        .ui32DlaneHsPreTime = 0xC,
        .ui32DlaneHsZeroTime = 0x1B,
        .ui32DlaneHsTrailTime = 0x13,
        .ui32ClaneHsPreTime = 0x7,
        .ui32ClaneHsZeroTime = 0x35,
        .ui32ClaneHsTrailTime = 0xF,
        .ui32ClaneHsClkPreTime = 0x7,
        .ui32ClaneHsClkPostTime = 0x3F,
#endif
    },
    {
        // Refclk_in_sel = AM_HAL_DSI_25MHZ;
        // BitRate = 400Mbps;
        // SSCEn = 1;
        // SSCPRD = 30.0KMz;
        // SSCDelta = 2500.0ppm;
        .ui32DataLaneHS_b.ui32DataLaneHSPreTime = 0xB,
        .ui32DataLaneHS_b.ui32DataLaneHSZeroTime = 0x18,
        .ui32DataLaneHS_b.ui32DataLaneHSTrailTime = 0x12,
        .ui32DataLaneHS_b.ui32ClockLaneHSPreTime = 0x7,
        .ui32ClockLaneHS_b.ui32ClockLaneHSZeroTime = 0x36,
        .ui32ClockLaneHS_b.ui32ClockLaneHSTrailTime = 0x10,
        .ui32ClockLaneHS_b.ui32ClockLaneHSCLKPreTime = 0x4,
        .ui32ClockLaneHS_b.ui32ClockLaneHSCLKPostTime = 0x26,
        .ui32Time_b.ui32TexitTime = 0x14,
        .ui32Time_b.ui32HSTLPTime = 0xA,

        .ui32Time_b.ui32ExtdCycleSel = 0x2,
        .ui32PLLFBK_b.ui32PLLPreDiv = 0x1,
        .ui32PLLFBK_b.ui32PLLFBKInit = 0x80,
        .ui32PLLFBK_b.ui32PLLPosDiv = 0x1,
        .ui32PLLFBKFRA_b.ui32PLLFBKFRA = 0x0,
        .ui32PLLSSCDelta_b.ui32PLLSSCDelta = 0xC9A,
        .ui32PLL_b.ui32PLLSSCPRD = 0x68,
        .ui32BitRate = 400000000,
#ifdef ATOMIQ11X_FPGA
        .ui32PllPreDiv = 0x1,
        .ui32ExtdCycleSet = 0x2,
        .ui32PllFbkInt = 0x80,
        .ui32PllFbkFra = 0x00,
        .ui32DlaneHsPreTime = 0xA,
        .ui32DlaneHsZeroTime = 0x18,
        .ui32DlaneHsTrailTime = 0x11,
        .ui32ClaneHsPreTime = 0x7,
        .ui32ClaneHsZeroTime = 0x35,
        .ui32ClaneHsTrailTime = 0xF,
        .ui32ClaneHsClkPreTime = 0x3,
        .ui32ClaneHsClkPostTime = 0x25,
#endif
    },
};

//*****************************************************************************
//
//! @brief Assert / deassert the panel reset GPIO sequence.
//
//*****************************************************************************
void
am_devices_display_fl7703ni_hardware_reset()
{
    am_bsp_disp_reset_pins_set();
    DELAY(5);
    am_bsp_disp_reset_pins_clear();
    DELAY(20);
    am_bsp_disp_reset_pins_set();
    DELAY(150);
}

//*****************************************************************************
//
//! @brief Fill default DC DPI / DSI video / panel settings.
//
//*****************************************************************************
uint32_t
am_devices_display_fl7703ni_get_default_config(am_hal_dc_config_t *pDCCfg,
                                               am_hal_ddb_config_t *pUnused,
                                               am_hal_dsi_config_t *pDSICfg,
                                               am_devices_dc_config_t *pDevCfg)
{
    (void)pUnused;
    g_sDispCfg.eInterface = DISP_IF_DSI;
    if (gsDSI.eVidPixelFormat == AM_HAL_DSI_VIDEO_PIXEL_FORMAT_RGB_888_24BIT)
    {
        gsDCDPI.ui32DstFormat = AM_HAL_DC_FORMAT_DPI_RGB888;
        gsDCDPI.sDPICfg.sTiming.fpx = 56;   //!< HFP
        gsDCDPI.sDPICfg.sTiming.bpx = 3;    //!< HBP
    }
    else if (gsDSI.eVidPixelFormat == AM_HAL_DSI_VIDEO_PIXEL_FORMAT_RGB_565_16BIT)
    {
        gsDCDPI.ui32DstFormat = AM_HAL_DC_FORMAT_DPI_RGB565_CFG1;
        gsDCDPI.sDPICfg.sTiming.fpx = 84;   //!< HFP
        gsDCDPI.sDPICfg.sTiming.bpx = 100;  //!< HBP
    }
    else
    {
        // Unsupported DSI video pixel format for this panel table.
        while (1);
    }

    //
    // DPI gearbox requires active-high sync polarity.
    //


    gsDSI.sPLLTimeCfg = sPllTimecfgs[0];
    gsDSI.sVidTiming.ui16VidHACT = gsDCDPI.sDPICfg.sTiming.resx;
    gsDSI.sVidTiming.ui16VidHFP = gsDCDPI.sDPICfg.sTiming.fpx;
    gsDSI.sVidTiming.ui16VidHBP = gsDCDPI.sDPICfg.sTiming.bpx;
    gsDSI.sVidTiming.ui16VidHSA = gsDCDPI.sDPICfg.sTiming.blx;
    gsDSI.sVidTiming.ui16VidVBP = gsDCDPI.sDPICfg.sTiming.bpy;
    gsDSI.sVidTiming.ui16VidVFP = gsDCDPI.sDPICfg.sTiming.fpy;
    gsDSI.sVidTiming.ui16VidVACT = gsDCDPI.sDPICfg.sTiming.resy;
    gsDSI.sVidTiming.ui16VidVSA = gsDCDPI.sDPICfg.sTiming.bly;

    if (pDCCfg != NULL)
    {
        *pDCCfg = gsDCDPI;
    }

    if (pDSICfg != NULL)
    {
        *pDSICfg = gsDSI;
    }

    if (pDevCfg != NULL)
    {
        pDSICfg->ui8LanesNum = g_sDispCfg.ui8NumLanes;
        pDevCfg->ui8Lanes = pDSICfg->ui8LanesNum;
        //
        // Set default panel configuration
        //
        pDevCfg->ui16ResX = pDCCfg->sDPICfg.sTiming.resx;
        pDevCfg->ui16ResY = pDCCfg->sDPICfg.sTiming.resy;
        pDevCfg->bTEEnable = pDCCfg->bTEEnable;
        pDevCfg->ui16MinX = 0;
        pDevCfg->ui16MinY = 0;
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Blocking panel register transfer via the DSI APB packet interface.
//!
//! Does not use DC or DBIDSIBRIDGE.
//
//*****************************************************************************
static uint32_t
blocking_transfer(void *pHandle, am_hal_dsi_transfer_t *pTransfer,
                  uint32_t ui32TimeoutMs)
{
    am_devices_fl7703ni_t *pDevice = (am_devices_fl7703ni_t *)pHandle;
    return am_hal_dsi_blocking_transfer(pDevice->pDSIHandle, pTransfer,
                                        ui32TimeoutMs * 1000);
}

//*****************************************************************************
//
//! @brief Send FL7703NI bring-up commands over the DSI APB packet interface.
//!
//! Pixel streaming remains the caller's responsibility (DC DPI + DSI video).
//
//*****************************************************************************
uint32_t
am_devices_display_fl7703ni_init(uint32_t ui32Module,
                                 void **ppHandle,
                                 void *pDCHandle,
                                 void *pUnused,
                                 void *pDSIHandle,
                                 am_devices_dc_config_t *pDevCfg)
{
    uint8_t ui8CmdBuf[64];
    uint32_t ui32Status;

    am_hal_dsi_transfer_t DSITransaction = {0};

    DSITransaction.ui32PktControl_b.ui32PktHSLPMode = DSI_IN_HS_MODE;
    DSITransaction.pui32TxPayload = (uint32_t *)ui8CmdBuf;

    if ( ui32Module > AM_REG_DC_NUM_MODULES )
    {
        return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    am_devices_fl7703ni_t *pDevice = &gFl7703ni[ui32Module];
    pDevice->pDCHandle = pDCHandle;
    pDevice->pDSIHandle = pDSIHandle;
    //
    // Enable User command:F1h, 12h, 87h
    // Disable User command:00h, 00h, 00h
    //
    const int MIPI_set_extc = 0xB9;
    ui8CmdBuf[0] = MIPI_set_extc;
    ui8CmdBuf[1] = 0xF1;
    ui8CmdBuf[2] = 0x12;
    ui8CmdBuf[3] = 0x87;
    DSITransaction.ui32PktControl_b.ui32PktWC = 4;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Control the display resolution
    //
    const int MIPI_set_disp = 0xB2;
    ui8CmdBuf[0] = MIPI_set_disp;
    ui8CmdBuf[1] = 0x78;
    ui8CmdBuf[2] = 0x14;
    ui8CmdBuf[3] = 0x70;
    DSITransaction.ui32PktControl_b.ui32PktWC = 4;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Control RGB I/F porch timing for internal use
    //
    const int MIPI_set_rgbif = 0xB3;
    ui8CmdBuf[0] = MIPI_set_rgbif;
    ui8CmdBuf[1] = 0x10;    // VBP_RGB_GEN
    ui8CmdBuf[2] = 0x10;    // VFP_RGB_GEN
    ui8CmdBuf[3] = 0x28;    // DE_BP_RGB_GEN
    ui8CmdBuf[4] = 0x28;    // DE_FP_RGB_GEN
    // the rest is undocumented in FL7703NI and ST7703 datasheets.
    ui8CmdBuf[5] = 0x03;    //
    ui8CmdBuf[6] = 0xFF;    //
    ui8CmdBuf[7] = 0x00;    //
    ui8CmdBuf[8] = 0x00;    //
    ui8CmdBuf[9] = 0x00;    //
    ui8CmdBuf[10] = 0x00;   //
    DSITransaction.ui32PktControl_b.ui32PktWC = 11;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Control display inversion type
    //
    const int MIPI_set_cyc = 0xB4;
    ui8CmdBuf[0] = MIPI_set_cyc;
    ui8CmdBuf[1] = 0x80;
    DSITransaction.ui32PktControl_b.ui32PktWC = ui8CmdBuf[1] << 8 | ui8CmdBuf[0];
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Internal reference voltage setting.
    //
    const int MIPI_set_bgp = 0xB5;
    ui8CmdBuf[0] = MIPI_set_bgp;
    ui8CmdBuf[1] = 0x0A;    // VREF
    ui8CmdBuf[2] = 0x0A;    // NVREF
    DSITransaction.ui32PktControl_b.ui32PktWC = 3;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set VCOM Voltage
    //
    const int MIPI_set_vcom = 0xB6;
    ui8CmdBuf[0] = MIPI_set_vcom;
    ui8CmdBuf[1] = 0x70;    // VCOMDC_F
    ui8CmdBuf[2] = 0x70;    // VCOMDC_B
    DSITransaction.ui32PktControl_b.ui32PktWC = 3;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set display related register
    //
    const int MIPI_set_power_ext = 0xB8;
    ui8CmdBuf[0] = MIPI_set_power_ext;
    ui8CmdBuf[1] = 0x26;
    DSITransaction.ui32PktControl_b.ui32PktWC = ui8CmdBuf[1] << 8 | ui8CmdBuf[0];
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set MIPI related register
    //
    const int MIPI_set_mipi = 0xBA;
    ui8CmdBuf[0] = MIPI_set_mipi;
    //
    // 00h - 1 lane
    // 01h - 2 lanes
    //
    if (pDevCfg->ui8Lanes == 1)
    {
        ui8CmdBuf[1] = 0x30;
    }
    else if (pDevCfg->ui8Lanes == 2)
    {
        ui8CmdBuf[1] = 0x31;
    }
    else
    {
        return AM_DEVICES_DISPLAY_STATUS_INVALID_ARG;
    }
    ui8CmdBuf[2] = 0x81;
    ui8CmdBuf[3] = 0x05;
    ui8CmdBuf[4] = 0xF9;
    ui8CmdBuf[5] = 0xFF;    //0x0E;
    ui8CmdBuf[6] = 0x0A;    //0x0E;
    // the rest is undocumented in FL7703NI and ST7703 datasheets.
    ui8CmdBuf[7] = 0x20;
    ui8CmdBuf[8] = 0x00;
    ui8CmdBuf[9] = 0x00;
    ui8CmdBuf[10] = 0x00;
    ui8CmdBuf[11] = 0x00;
    ui8CmdBuf[12] = 0x00;
    ui8CmdBuf[13] = 0x00;
    ui8CmdBuf[14] = 0x00;
    ui8CmdBuf[15] = 0x44;
    ui8CmdBuf[16] = 0x25;
    ui8CmdBuf[17] = 0x00;
    ui8CmdBuf[18] = 0x91;
    ui8CmdBuf[19] = 0x0A;
    ui8CmdBuf[20] = 0x00;
    ui8CmdBuf[21] = 0x00;
    ui8CmdBuf[22] = 0x01;
    ui8CmdBuf[23] = 0x4F;
    ui8CmdBuf[24] = 0x01;
    ui8CmdBuf[25] = 0x00;
    ui8CmdBuf[26] = 0x00;
    ui8CmdBuf[27] = 0x37;
    DSITransaction.ui32PktControl_b.ui32PktWC = 28;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Control NVDDD/VDDD Voltage
    //
    const int MIPI_set_vdc = 0xBC;
    ui8CmdBuf[0] = MIPI_set_vdc;
    ui8CmdBuf[1] = 0x47;
    DSITransaction.ui32PktControl_b.ui32PktWC = ui8CmdBuf[1] << 8 | ui8CmdBuf[0];
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Unknown BF
    //
    const int MIPI_set_bf = 0xBF;
    ui8CmdBuf[0] = MIPI_set_bf;
    ui8CmdBuf[1] = 0x02;
    ui8CmdBuf[2] = 0x11;
    ui8CmdBuf[3] = 0x00;
    DSITransaction.ui32PktControl_b.ui32PktWC = 4;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set Source driving related setting
    //
    const int MIPI_set_scr = 0xC0;
    ui8CmdBuf[0] = MIPI_set_scr;
    ui8CmdBuf[1] = 0x73;
    ui8CmdBuf[2] = 0x73;
    ui8CmdBuf[3] = 0x50;
    ui8CmdBuf[4] = 0x50;
    ui8CmdBuf[5] = 0x00;
    ui8CmdBuf[6] = 0x00;
    ui8CmdBuf[7] = 0x12;
    ui8CmdBuf[8] = 0x73;
    ui8CmdBuf[9] = 0x00;
    DSITransaction.ui32PktControl_b.ui32PktWC = 10;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set Power related setting
    //
    const int MIPI_set_power = 0xC1;
    ui8CmdBuf[0] = MIPI_set_power;
    ui8CmdBuf[1] = 0x54;
    ui8CmdBuf[2] = 0x00;
    ui8CmdBuf[3] = 0x32;
    ui8CmdBuf[4] = 0x32;
    ui8CmdBuf[5] = 0x77;
    ui8CmdBuf[6] = 0xF4;
    ui8CmdBuf[7] = 0x77;
    ui8CmdBuf[8] = 0x77;
    ui8CmdBuf[9] = 0xCC;
    ui8CmdBuf[10] = 0xCC;
    ui8CmdBuf[11] = 0xFF;
    ui8CmdBuf[12] = 0xFF;
    ui8CmdBuf[13] = 0x11;
    ui8CmdBuf[14] = 0x11;
    ui8CmdBuf[15] = 0x00;
    ui8CmdBuf[16] = 0x00;
    ui8CmdBuf[17] = 0x31;
    DSITransaction.ui32PktControl_b.ui32PktWC = 18;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set ID
    //
    const int MIPI_set_id = 0xC3;
    ui8CmdBuf[0] = MIPI_set_id;
    ui8CmdBuf[1] = 0x38;
    ui8CmdBuf[2] = 0x21;
    ui8CmdBuf[3] = 0x1F;
    ui8CmdBuf[4] = 0x00;
    DSITransaction.ui32PktControl_b.ui32PktWC = 5;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);
    //
    // Set I/O related register
    //
    const int MIPI_set_io = 0xC7;
    ui8CmdBuf[0] = MIPI_set_io;
    ui8CmdBuf[1] = 0x18;    // Enabled Vout & Hout signal
    ui8CmdBuf[2] = 0x00;
    ui8CmdBuf[3] = 0x0A;
    ui8CmdBuf[4] = 0x00;
    ui8CmdBuf[5] = 0x00;
    ui8CmdBuf[6] = 0x00;
    ui8CmdBuf[7] = 0x00;
    ui8CmdBuf[8] = 0x00;
    ui8CmdBuf[9] = 0xED;
    ui8CmdBuf[10] = 0xC7;
    ui8CmdBuf[11] = 0x00;
    ui8CmdBuf[12] = 0xA5;
    DSITransaction.ui32PktControl_b.ui32PktWC = 13;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set CABC related register
    //
    const int MIPI_set_cabc = 0xC8;
    ui8CmdBuf[0] = MIPI_set_cabc;
    ui8CmdBuf[1] = 0x10;
    ui8CmdBuf[2] = 0x40;
    ui8CmdBuf[3] = 0x1E;
    ui8CmdBuf[4] = 0x03;
    DSITransaction.ui32PktControl_b.ui32PktWC = 5;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set clock related register
    //
    const int MIPI_set_clock = 0xCB;
    ui8CmdBuf[0] = MIPI_set_clock;
    ui8CmdBuf[1] = 0x03;
    DSITransaction.ui32PktControl_b.ui32PktWC = ui8CmdBuf[1] << 8 | ui8CmdBuf[0];
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set display related register
    //
    const int MIPI_set_panel = 0xCC;
    ui8CmdBuf[0] = MIPI_set_panel;
    ui8CmdBuf[1] = 0x0B;
    DSITransaction.ui32PktControl_b.ui32PktWC = ui8CmdBuf[1] << 8 | ui8CmdBuf[0];
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set Gamma characteristics
    //
    const int MIPI_set_gamma = 0xE0;
    ui8CmdBuf[0] = MIPI_set_gamma;
    ui8CmdBuf[1] = 0x00;
    ui8CmdBuf[2] = 0x05;
    ui8CmdBuf[3] = 0x09;
    ui8CmdBuf[4] = 0x29;
    ui8CmdBuf[5] = 0x3C;
    ui8CmdBuf[6] = 0x3F;
    ui8CmdBuf[7] = 0x3B;
    ui8CmdBuf[8] = 0x37;
    ui8CmdBuf[9] = 0x05;
    ui8CmdBuf[10] = 0x0A;
    ui8CmdBuf[11] = 0x0C;
    ui8CmdBuf[12] = 0x10;
    ui8CmdBuf[13] = 0x13;
    ui8CmdBuf[14] = 0x10;
    ui8CmdBuf[15] = 0x13;
    ui8CmdBuf[16] = 0x12;
    ui8CmdBuf[17] = 0x1A;
    ui8CmdBuf[18] = 0x00;
    ui8CmdBuf[19] = 0x05;
    ui8CmdBuf[20] = 0x09;
    ui8CmdBuf[21] = 0x29;
    ui8CmdBuf[22] = 0x3C;
    ui8CmdBuf[23] = 0x3F;
    ui8CmdBuf[24] = 0x3B;
    ui8CmdBuf[25] = 0x37;
    ui8CmdBuf[26] = 0x05;
    ui8CmdBuf[27] = 0x0A;
    ui8CmdBuf[28] = 0x0C;
    ui8CmdBuf[29] = 0x10;
    ui8CmdBuf[30] = 0x13;
    ui8CmdBuf[31] = 0x10;
    ui8CmdBuf[32] = 0x13;
    ui8CmdBuf[33] = 0x12;
    ui8CmdBuf[34] = 0x1A;
    DSITransaction.ui32PktControl_b.ui32PktWC = 35;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Unknown E1
    //
    const int MIPI_set_e1 = 0xE1;
    ui8CmdBuf[0] = MIPI_set_e1;
    ui8CmdBuf[1] = 0x11;
    ui8CmdBuf[2] = 0x11;
    ui8CmdBuf[3] = 0x91;
    ui8CmdBuf[4] = 0x00;
    ui8CmdBuf[5] = 0x00;
    ui8CmdBuf[6] = 0x00;
    ui8CmdBuf[7] = 0x00;
    DSITransaction.ui32PktControl_b.ui32PktWC = 8;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set EQ related register
    //
    const int MIPI_set_eq = 0xE3;
    ui8CmdBuf[0] = MIPI_set_eq;
    ui8CmdBuf[1] = 0x07;
    ui8CmdBuf[2] = 0x07;
    ui8CmdBuf[3] = 0x0B;
    ui8CmdBuf[4] = 0x0B;
    ui8CmdBuf[5] = 0x03;
    ui8CmdBuf[6] = 0x0B;
    ui8CmdBuf[7] = 0x00;
    ui8CmdBuf[8] = 0x00;
    ui8CmdBuf[9] = 0x00;
    ui8CmdBuf[10] = 0x00;
    ui8CmdBuf[11] = 0xFF;
    ui8CmdBuf[12] = 0x04;
    ui8CmdBuf[13] = 0xC0;
    ui8CmdBuf[14] = 0x10;
    DSITransaction.ui32PktControl_b.ui32PktWC = 15;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set forward GIP timing
    //
    const int MIPI_set_gip1 = 0xE9;
    ui8CmdBuf[0] = MIPI_set_gip1;
    ui8CmdBuf[1] = 0x01;
    ui8CmdBuf[2] = 0x00;
    ui8CmdBuf[3] = 0x0E;
    ui8CmdBuf[4] = 0x00;
    ui8CmdBuf[5] = 0x00;
    ui8CmdBuf[6] = 0xB0;
    ui8CmdBuf[7] = 0xB1;
    ui8CmdBuf[8] = 0x11;
    ui8CmdBuf[9] = 0x31;
    ui8CmdBuf[10] = 0x23;
    ui8CmdBuf[11] = 0x28;
    ui8CmdBuf[12] = 0x10;
    ui8CmdBuf[13] = 0xB0;
    ui8CmdBuf[14] = 0xB1;
    ui8CmdBuf[15] = 0x27;
    ui8CmdBuf[16] = 0x08;
    ui8CmdBuf[17] = 0x00;
    ui8CmdBuf[18] = 0x04;
    ui8CmdBuf[19] = 0x02;
    ui8CmdBuf[20] = 0x00;
    ui8CmdBuf[21] = 0x00;
    ui8CmdBuf[22] = 0x00;
    ui8CmdBuf[23] = 0x00;
    ui8CmdBuf[24] = 0x04;
    ui8CmdBuf[25] = 0x02;
    ui8CmdBuf[26] = 0x00;
    ui8CmdBuf[27] = 0x00;
    ui8CmdBuf[28] = 0x00;
    ui8CmdBuf[29] = 0x88;
    ui8CmdBuf[30] = 0x88;
    ui8CmdBuf[31] = 0xBA;
    ui8CmdBuf[32] = 0x60;
    ui8CmdBuf[33] = 0x24;
    ui8CmdBuf[34] = 0x08;
    ui8CmdBuf[35] = 0x88;
    ui8CmdBuf[36] = 0x88;
    ui8CmdBuf[37] = 0x88;
    ui8CmdBuf[38] = 0x88;
    ui8CmdBuf[39] = 0x88;
    ui8CmdBuf[40] = 0x88;
    ui8CmdBuf[41] = 0x88;
    ui8CmdBuf[42] = 0xBA;
    ui8CmdBuf[43] = 0x71;
    ui8CmdBuf[44] = 0x35;
    ui8CmdBuf[45] = 0x18;
    ui8CmdBuf[46] = 0x88;
    ui8CmdBuf[47] = 0x88;
    ui8CmdBuf[48] = 0x88;
    ui8CmdBuf[49] = 0x88;
    ui8CmdBuf[50] = 0x88;
    ui8CmdBuf[51] = 0x00;
    ui8CmdBuf[52] = 0x00;
    ui8CmdBuf[53] = 0x00;
    ui8CmdBuf[54] = 0x01;
    ui8CmdBuf[55] = 0x00;
    ui8CmdBuf[56] = 0x00;
    ui8CmdBuf[57] = 0x00;
    ui8CmdBuf[58] = 0x00;
    ui8CmdBuf[59] = 0x00;
    ui8CmdBuf[60] = 0x00;
    ui8CmdBuf[61] = 0x00;
    ui8CmdBuf[62] = 0x00;
    ui8CmdBuf[63] = 0x00;
    DSITransaction.ui32PktControl_b.ui32PktWC = 64;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Set backward GIP timing
    //
    const int MIPI_set_gip2 = 0xEA;
    ui8CmdBuf[0] = MIPI_set_gip2;
    ui8CmdBuf[1] = 0x97;
    ui8CmdBuf[2] = 0x0A;
    ui8CmdBuf[3] = 0x82;
    ui8CmdBuf[4] = 0x02;
    ui8CmdBuf[5] = 0x03;
    ui8CmdBuf[6] = 0x07;
    ui8CmdBuf[7] = 0x00;
    ui8CmdBuf[8] = 0x00;
    ui8CmdBuf[9] = 0x00;
    ui8CmdBuf[10] = 0x00;
    ui8CmdBuf[11] = 0x00;
    ui8CmdBuf[12] = 0x00;
    ui8CmdBuf[13] = 0x81;
    ui8CmdBuf[14] = 0x88;
    ui8CmdBuf[15] = 0xBA;
    ui8CmdBuf[16] = 0x17;
    ui8CmdBuf[17] = 0x53;
    ui8CmdBuf[18] = 0x88;
    ui8CmdBuf[19] = 0x88;
    ui8CmdBuf[20] = 0x88;
    ui8CmdBuf[21] = 0x88;
    ui8CmdBuf[22] = 0x88;
    ui8CmdBuf[23] = 0x88;
    ui8CmdBuf[24] = 0x80;
    ui8CmdBuf[25] = 0x88;
    ui8CmdBuf[26] = 0xBA;
    ui8CmdBuf[27] = 0x06;
    ui8CmdBuf[28] = 0x42;
    ui8CmdBuf[29] = 0x88;
    ui8CmdBuf[30] = 0x88;
    ui8CmdBuf[31] = 0x88;
    ui8CmdBuf[32] = 0x88;
    ui8CmdBuf[33] = 0x88;
    ui8CmdBuf[34] = 0x88;
    ui8CmdBuf[35] = 0x23;
    ui8CmdBuf[36] = 0x00;
    ui8CmdBuf[37] = 0x00;
    ui8CmdBuf[38] = 0x02;
    ui8CmdBuf[39] = 0x80;
    ui8CmdBuf[40] = 0x00;
    ui8CmdBuf[41] = 0x00;
    ui8CmdBuf[42] = 0x00;
    ui8CmdBuf[43] = 0x00;
    ui8CmdBuf[44] = 0x00;
    ui8CmdBuf[45] = 0x00;
    ui8CmdBuf[46] = 0x00;
    ui8CmdBuf[47] = 0x00;
    ui8CmdBuf[48] = 0x00;
    ui8CmdBuf[49] = 0x00;
    ui8CmdBuf[50] = 0x00;
    ui8CmdBuf[51] = 0x00;
    ui8CmdBuf[52] = 0x00;
    ui8CmdBuf[53] = 0x00;
    ui8CmdBuf[54] = 0x00;
    ui8CmdBuf[55] = 0x00;
    ui8CmdBuf[56] = 0x00;
    ui8CmdBuf[57] = 0x00;
    ui8CmdBuf[58] = 0x00;
    ui8CmdBuf[59] = 0x00;
    ui8CmdBuf[60] = 0x00;
    ui8CmdBuf[61] = 0x00;
    DSITransaction.ui32PktControl_b.ui32PktWC = 62;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Unknown EF
    //
    const int MIPI_set_ef = 0xEF;
    ui8CmdBuf[0] = MIPI_set_ef;
    ui8CmdBuf[1] = 0xFF;
    ui8CmdBuf[2] = 0xFF;
    ui8CmdBuf[3] = 0x01;
    DSITransaction.ui32PktControl_b.ui32PktWC = 4;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(10);

    //
    // Exit sleep mode and turn the panel on.
    //
    DSITransaction.ui32PktControl_b.ui32PktWC = AM_HAL_DC_CMD_TYPE_EXIT_SLEEP_MODE;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_0_PARAMETER;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(130);


    DSITransaction.ui32PktControl_b.ui32PktWC = AM_HAL_DC_CMD_TYPE_SET_DISPLAY_ON;
    DSITransaction.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_0_PARAMETER;
    ui32Status = blocking_transfer((void *)pDevice, &DSITransaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(200);

    pDevice->ui32Module = ui32Module;
    pDevice->bOccupied = true;
    *ppHandle = (void *)pDevice;

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
