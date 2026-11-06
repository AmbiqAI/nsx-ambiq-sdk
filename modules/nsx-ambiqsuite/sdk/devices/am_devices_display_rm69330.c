//*****************************************************************************
//
//! @file am_devices_display_rm69330.c
//!
//! @brief Driver for Raydium RM69330 (and compatible) panels.
//!
//! Supports SPI4/DSPI/QSPI via DC, and MIPI-DSI command mode. On DSI:
//!   - Panel register packets: DSI APB packet interface, or DC + DBIDSIBRIDGE
//!     (Tx/Rx packet path), selected by DSI_APB_PACKET_INTERFACE.
//!   - Frame pixels: always DC (DBI) through DBIDSIBRIDGE into DSI.
//!
//! Configures tearing, pixel format, DSPI mode, brightness, and the update
//! window via MIPI DCS / manufacturer commands.
//!
//! @addtogroup display_rm69330 RM69330 Display Device Driver
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

#include "am_devices_display_rm69330.h"
#include "am_util_delay.h"
#include "am_bsp.h"
#include "am_hal_dc.h"
#include "am_hal_dsi.h"
#include "am_hal_ddb.h"

//
// Panel command sequences require millisecond delays between steps.
//
#ifndef SIMULATION
#define DELAY am_util_delay_ms
#else
#define DELAY(...)
#endif

#define RM69330_MIPI_SET_CMD_PAGE           0xFE
#define RM69330_MIPI_SET_HSIFOPCTR          0x0A
#define RM69330_MIPI_SET_DSPI_MODE          0xC4
#define RM69330_MIPI_SET_WR_DISPLAY_CTRL    0x53
#define RM69330_MIPI_SET_0xF4               0xF4
#define RM69330_MIPI_SET_0xF5               0xF5
#define RM69330_MIPI_SET_0x00               0x00
//! Per-instance driver state for RM69330.
typedef struct
{
    uint32_t                    ui32Module;   //!< Module index.
    void                        *pDCHandle;   //!< Display controller handle.
    void                        *pDDBHandle;  //!< DBIDSIBRIDGE handle (DSI).
    void                        *pDSIHandle;  //!< DSI handle.
    bool                        bOccupied;    //!< True after successful init.
}
am_devices_rm69330_t;

am_devices_rm69330_t gRm69330[AM_REG_DC_NUM_MODULES];

typedef struct
{
    //
    // DC transfer descriptor. Used for QSPI/JDI, and for DSI command-mode frame
    // pushes (WRITE_MEMORY_START / CONTINUE). Panel register commands that use
    // the DSI APB packet interface do not use this field.
    //
    am_hal_dc_transfer_t DCTransfer;
#if !DSI_APB_PACKET_INTERFACE
    //
    // DSI Tx/Rx packet path: panel commands go DC DBI -> DBIDSIBRIDGE (DDB) ->
    // DSI. DDBLutCfg programs the bridge LUT for each command/read.
    //
    am_hal_ddb_lut_config_t DDBLutCfg;
#else
    //
    // DSI APB packet path: panel register write/read packets are issued by the
    // DSI host APB packet interface and do not go through DC or DBIDSIBRIDGE.
    // Frame pixels still use DCTransfer above.
    //
    am_hal_dsi_transfer_t DSITransfer;
#endif
} am_devices_transfer_t;


//! Default DC configuration for QSPI / DSPI / SPI4.
am_hal_dc_config_t gsDCQSPI =
{
    .eInterface = AM_HAL_DC_IF_QSPI,
    .bTEEnable = false,
    .ui32DstFormat = AM_HAL_DC_FORMAT_RGB888,
    .sSPICfg.sTiming.fpx = 1,
    .sSPICfg.sTiming.fpy = 1,
    .sSPICfg.sTiming.bpx = 1,
    .sSPICfg.sTiming.bpy = 1,
    .sSPICfg.sTiming.blx = 1,
    .sSPICfg.sTiming.bly = 1,
    .sSPICfg.ui32PolPha = 0,
    .sSPICfg.bDDREnable = false,

#ifdef ATOMIQ11X_FPGA
    .sSPICfg.fTargetClkWrite = ATOMIQ11X_FPGA,
    .sSPICfg.fTargetClkRead = 10.0,
#else
    .sSPICfg.fTargetClkWrite = 48.0,
    .sSPICfg.fTargetClkRead = 10.0,
#endif
};

//! Default DC DBI configuration for MIPI-DSI command-mode pixel path.
am_hal_dc_config_t gsDCDBI =
{
    .eInterface = AM_HAL_DC_IF_DBI,
    .bTEEnable = false,
    .ui32DstFormat = AM_HAL_DC_IF_CFG_DBI16 | AM_HAL_DC_FORMAT_RGB888,
    .sDBICfg.sTiming.fpx = 1,
    .sDBICfg.sTiming.fpy = 1,
    .sDBICfg.sTiming.bpx = 1,
    .sDBICfg.sTiming.bpy = 1,
    .sDBICfg.sTiming.blx = 1,
    .sDBICfg.sTiming.bly = 1,
#ifdef ATOMIQ11X_FPGA
    .sDBICfg.fTargetClkWRX = ATOMIQ11X_FPGA,
#else
    .sDBICfg.fTargetClkWRX = 96.0,
#endif
};

//! Default DSI host configuration (command mode; video disabled).
am_hal_dsi_config_t gsDSI =
{
    .ui8LanesNum = 1,
    .bContinuousClock = false,
    .ui8TXGAP = 0x4,
    .bAutoInsertEOTP = true,
#ifdef ATOMIQ11X_FPGA
    .bPPI16EN = false,          //!< FPGA M31 interposer supports 8-bit PPI only.
#else
    .bPPI16EN = true,
#endif
    .bVidEnable = false,
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
am_devices_display_rm69330_hardware_reset()
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
//! @brief Fill default DC / DDB / DSI / panel settings from BSP and static tables.
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_get_default_config(am_hal_dc_config_t *pDCCfg,
                                              am_hal_ddb_config_t *pDDBCfg,
                                              am_hal_dsi_config_t *pDSICfg,
                                              am_devices_dc_config_t *pDevCfg)
{
    am_hal_dc_config_t sDCCfg;
    am_hal_dsi_config_t sDSICfg;
    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
        sDCCfg = gsDCDBI;
        sDCCfg.sDBICfg.sTiming.resx = g_sDispCfg.ui16ResX;
        sDCCfg.sDBICfg.sTiming.resy = g_sDispCfg.ui16ResY;
        am_hal_ddb_config_t sDDBCfg;

        sDDBCfg.ui32DBI16_b.ui32DBI16 = ((sDCCfg.ui32DstFormat & AM_HAL_DC_IF_CFG_DBI16) != 0x0) ?\
                                        AM_HAL_DDB_DBI_WIDTH_16 : AM_HAL_DDB_DBI_WIDTH_8;
        sDDBCfg.ui32LinkMode = AM_HAL_DDB_DSI_LINK_MODE_POLICY;
        sDDBCfg.ui32VC = AM_HAL_DDB_DSI_VC_0;
        sDDBCfg.ui32ContCtrl = AM_HAL_DDB_CONT_EN;

        //
        // Continuous transfer byte count for Generic/DCS over DBI-8/DBI-16.
        // Must be < 511; larger values generally improve throughput.
        //
        sDDBCfg.ui32ContNBytes = 510;

        sDSICfg = gsDSI;
        sDSICfg.ui8LanesNum = g_sDispCfg.ui8NumLanes;
        sDSICfg.sPLLTimeCfg = sPllTimecfgs[0];
        if (pDSICfg != NULL)
        {
            *pDSICfg = sDSICfg;
        }
        if (pDDBCfg != NULL)
        {
            *pDDBCfg = sDDBCfg;
        }
    }
    else
    {
        if (g_sDispCfg.eInterface == DISP_IF_QSPI)
        {
            gsDCQSPI.eInterface = AM_HAL_DC_IF_QSPI;
        }
        else if (g_sDispCfg.eInterface == DISP_IF_DSPI)
        {
            gsDCQSPI.eInterface = AM_HAL_DC_IF_DSPI;
        }
        else if (g_sDispCfg.eInterface == DISP_IF_SPI4)
        {
            gsDCQSPI.eInterface = AM_HAL_DC_IF_SPI4;
        }

        sDCCfg = gsDCQSPI;
        sDCCfg.sSPICfg.sTiming.resx = g_sDispCfg.ui16ResX;
        sDCCfg.sSPICfg.sTiming.resy = g_sDispCfg.ui16ResY;
    }

    if (pDCCfg != NULL)
    {
        *pDCCfg = sDCCfg;
    }

    if (pDevCfg != NULL)
    {
        //
        // Set default panel configuration
        //
        if (g_sDispCfg.eInterface == DISP_IF_DSI)
        {
            pDevCfg->ui16ResX = sDCCfg.sDBICfg.sTiming.resx;
            pDevCfg->ui16ResY = sDCCfg.sDBICfg.sTiming.resy;
        }
        else
        {
            pDevCfg->ui16ResX = sDCCfg.sSPICfg.sTiming.resx;
            pDevCfg->ui16ResY = sDCCfg.sSPICfg.sTiming.resy;
        }

        pDevCfg->ui16MinX = 0;
        pDevCfg->ui16MinX = 0;

        pDevCfg->bTEEnable = sDCCfg.bTEEnable;
        pDevCfg->ui32PixelFormat = sDCCfg.ui32DstFormat;
        pDevCfg->ui8Lanes = sDSICfg.ui8LanesNum;
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Blocking panel register transfer.
//!
//! On DSI with DSI_APB_PACKET_INTERFACE: uses the DSI APB packet interface only.
//! On DSI without it: programs DBIDSIBRIDGE LUT, then transfers via DC.
//! On SPI4/DSPI/QSPI: transfers via DC only.
//
//*****************************************************************************
static uint32_t
blocking_transfer(void *pHandle, am_devices_transfer_t *pTransfer, uint32_t ui32TimeoutMs)
{
    uint32_t ui32Status = AM_HAL_STATUS_SUCCESS;
    am_devices_rm69330_t *pDevice = (am_devices_rm69330_t *)pHandle;

    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
#if DSI_APB_PACKET_INTERFACE
        ui32Status = am_hal_dsi_blocking_transfer(pDevice->pDSIHandle, &pTransfer->DSITransfer, ui32TimeoutMs * 1000);
        DELAY(10);
        return ui32Status;
#else
        ui32Status = am_hal_ddb_lut_configure(pDevice->pDDBHandle, &pTransfer->DDBLutCfg, 1);
        if (ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return ui32Status;
        }
#endif
    }
    ui32Status = am_hal_dc_blocking_transfer(pDevice->pDCHandle, &pTransfer->DCTransfer, ui32TimeoutMs * 1000);
    DELAY(10);
    return ui32Status;
}

//*****************************************************************************
//
//! @brief Write MADCTL to flip along X, Y, or both.
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_flip(void *pHandle, uint8_t ui8FlipXY)
{
    uint8_t ui8CmdBuffer[4];
    am_devices_transfer_t Transaction = {0};

    ui8CmdBuffer[0] = ui8FlipXY;
    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
#if DSI_APB_PACKET_INTERFACE
        //
        // Panel register packet via DSI APB (does not use DCTransfer / DDB).
        //
        Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[0] << 8 | AM_HAL_DC_CMD_TYPE_SET_ADDRESS_MODE;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_1_PARAMETER;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktHSLPMode = DSI_IN_HS_MODE;
#else
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_ADDRESS_MODE;
        Transaction.DCTransfer.command.eDirection = AM_HAL_DC_WRITE;
        Transaction.DCTransfer.command.pui8TxBuf = ui8CmdBuffer;
        Transaction.DCTransfer.command.ui32TxLength = 1;

        am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_DCS_WRITE_DEFAULT;
        lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
        // DBI-16: high byte is a parameter when the Generic write byte count is odd.
        lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = Transaction.DCTransfer.command.ui32TxLength % 2;
        Transaction.DDBLutCfg.sCmdAttr = lutAttr;
        Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
    }
    else
    {
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_ADDRESS_MODE;
        Transaction.DCTransfer.command.eDirection = AM_HAL_DC_WRITE;
        Transaction.DCTransfer.command.pui8TxBuf = ui8CmdBuffer;
        Transaction.DCTransfer.command.ui32TxLength = 1;
    }
    uint32_t ui32Status = blocking_transfer(pHandle, &Transaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Panel bring-up command sequence for SPI4/DSPI/QSPI or MIPI-DSI.
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_init(uint32_t ui32Module,
                                void **ppHandle,
                                void *pDCHandle,
                                void *pDDBHandle,
                                void *pDSIHandle,
                                am_devices_dc_config_t *pDevCfg)
{
    uint32_t ui32Status;
    uint8_t ui8CmdBuffer[4];

    if ( ui32Module > AM_REG_DC_NUM_MODULES )
    {
        return AM_HAL_STATUS_OUT_OF_RANGE;
    }
    am_devices_transfer_t Transaction = {0};
    am_devices_rm69330_t *pDevice = &gRm69330[ui32Module];
    pDevice->pDCHandle = pDCHandle;
    pDevice->pDDBHandle = pDDBHandle;
    pDevice->pDSIHandle = pDSIHandle;

    Transaction.DCTransfer.command.eDirection = AM_HAL_DC_WRITE;
    Transaction.DCTransfer.command.pui8TxBuf = ui8CmdBuffer;

    Transaction.DSITransfer.ui32PktControl_b.ui32PktHSLPMode = DSI_IN_HS_MODE;
    Transaction.DSITransfer.pui32TxPayload = (uint32_t *)ui8CmdBuffer;

    //
    // Enable tearing effect (TE) on the panel.
    //
    ui8CmdBuffer[0] = 0x02;
    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
#if DSI_APB_PACKET_INTERFACE
        //
        // Panel register packet via DSI APB (does not use DCTransfer / DDB).
        //
        Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[0] << 8 | AM_HAL_DC_CMD_TYPE_SET_TEAR_ON;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_1_PARAMETER;
#else
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_TEAR_ON;
        Transaction.DCTransfer.command.ui32TxLength = 1;

        am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_DCS_WRITE_DEFAULT;
        lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
        // DBI-16: high byte is a parameter when the DCS write byte count is odd.
        lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = Transaction.DCTransfer.command.ui32TxLength % 2;
        Transaction.DDBLutCfg.sCmdAttr = lutAttr;
        Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
    }
    else
    {
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_TEAR_ON;
        Transaction.DCTransfer.command.ui32TxLength = 1;
    }
    ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    if (g_sDispCfg.eIC == DISP_IC_RM69330)
    {
        if (g_sDispCfg.eInterface == DISP_IF_DSI)
        {
            //
            // When using RM69330 DSI, set N565 reg to 1 to solve color issue caused by endianess.
            //
            ui8CmdBuffer[0] = RM69330_MIPI_SET_CMD_PAGE;
            ui8CmdBuffer[1] = 0x01; // MCS
#if DSI_APB_PACKET_INTERFACE
            //
            // Panel register packet via DSI APB (does not use DCTransfer / DDB).
            //
            Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[1] << 8 | ui8CmdBuffer[0];
            Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
#else
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_CMD_PAGE;
            Transaction.DCTransfer.command.ui32TxLength = 2;

            am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_WRITE_DEFAULT;
            lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
            // DBI-16: high byte is a parameter when the Generic write byte count is even.
            lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
            Transaction.DDBLutCfg.sCmdAttr = lutAttr;
            Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
            ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
            if ( ui32Status != AM_HAL_STATUS_SUCCESS )
            {
                return ui32Status;
            }

            ui8CmdBuffer[0] = RM69330_MIPI_SET_HSIFOPCTR;
            ui8CmdBuffer[1] = 0xF8; // set N565 to 1
#if DSI_APB_PACKET_INTERFACE
            //
            // Panel register packet via DSI APB (does not use DCTransfer / DDB).
            //
            Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[1] << 8 | ui8CmdBuffer[0];
            Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
#else
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_HSIFOPCTR;
            Transaction.DCTransfer.command.ui32TxLength = 2;

            lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
            // DBI-16: high byte is a parameter when the Generic write byte count is even.
            lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
            Transaction.DDBLutCfg.sCmdAttr = lutAttr;
            Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
            ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
            if ( ui32Status != AM_HAL_STATUS_SUCCESS )
            {
                return ui32Status;
            }

            ui8CmdBuffer[0] = RM69330_MIPI_SET_CMD_PAGE;
            ui8CmdBuffer[1] = 0x00; // UCS
#if DSI_APB_PACKET_INTERFACE
            //
            // Panel register packet via DSI APB (does not use DCTransfer / DDB).
            //
            Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[1] << 8 | ui8CmdBuffer[0];
            Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
#else
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_CMD_PAGE;
            Transaction.DCTransfer.command.ui32TxLength = 2;

            lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
            // DBI-16: high byte is a parameter when the Generic write byte count is even.
            lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
            Transaction.DDBLutCfg.sCmdAttr = lutAttr;
            Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
            ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
            if ( ui32Status != AM_HAL_STATUS_SUCCESS )
            {
                return ui32Status;
            }
        }
    }
    else if (g_sDispCfg.eIC == DISP_IC_CO5300)
    {
        //
        // Unlock vendor CMD2 page on CO5300-compatible controllers.
        //
        if (g_sDispCfg.eInterface == DISP_IF_DSI)
        {
            ui8CmdBuffer[0] = RM69330_MIPI_SET_CMD_PAGE;
            ui8CmdBuffer[1] = 0x20; // RFE 20
#if DSI_APB_PACKET_INTERFACE
            //
            // Panel register packet via DSI APB (does not use DCTransfer / DDB).
            //
            Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[1] << 8 | ui8CmdBuffer[0];
            Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
#else
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_CMD_PAGE;
            Transaction.DCTransfer.command.ui32TxLength = 2;

            am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_WRITE_DEFAULT;
            lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
            // DBI-16: high byte is a parameter when the Generic write byte count is even.
            lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
            Transaction.DDBLutCfg.sCmdAttr = lutAttr;
            Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
        }
        else
        {
            ui8CmdBuffer[0] = 0x20; // RFE 20
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_CMD_PAGE;
            Transaction.DCTransfer.command.ui32TxLength = 1;
        }
        ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            return ui32Status;
        }

        if (g_sDispCfg.eInterface == DISP_IF_DSI)
        {
            ui8CmdBuffer[0] = RM69330_MIPI_SET_0xF4;
            ui8CmdBuffer[1] = 0x5A;
#if DSI_APB_PACKET_INTERFACE
            //
            // Panel register packet via DSI APB (does not use DCTransfer / DDB).
            //
            Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[1] << 8 | ui8CmdBuffer[0];
            Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
#else
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_0xF4;
            Transaction.DCTransfer.command.ui32TxLength = 2;

            am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_WRITE_DEFAULT;
            lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
            // DBI-16: high byte is a parameter when the Generic write byte count is even.
            lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
            Transaction.DDBLutCfg.sCmdAttr = lutAttr;
            Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
        }
        else
        {
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_0xF4;
            ui8CmdBuffer[0] = 0x5A;
            Transaction.DCTransfer.command.ui32TxLength = 1;
        }
        ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            return ui32Status;
        }

        if (g_sDispCfg.eInterface == DISP_IF_DSI)
        {
            ui8CmdBuffer[0] = RM69330_MIPI_SET_0xF5;
            ui8CmdBuffer[1] = 0x59;
#if DSI_APB_PACKET_INTERFACE
            Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[1] << 8 | ui8CmdBuffer[0];
            Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
#else
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_0xF5;
            Transaction.DCTransfer.command.ui32TxLength = 2;

            am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_WRITE_DEFAULT;
            lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
            // DBI-16: high byte is a parameter when the Generic write byte count is even.
            lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
            Transaction.DDBLutCfg.sCmdAttr = lutAttr;
            Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
        }
        else
        {
            ui8CmdBuffer[0] = 0x59;
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_0xF5;
            Transaction.DCTransfer.command.ui32TxLength = 1;
        }
        ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            return ui32Status;
        }

        if (g_sDispCfg.eInterface == DISP_IF_DSI)
        {
            ui8CmdBuffer[0] = RM69330_MIPI_SET_CMD_PAGE;
            ui8CmdBuffer[1] = 0x80;
#if DSI_APB_PACKET_INTERFACE
            Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[1] << 8 | ui8CmdBuffer[0];
            Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
#else
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_CMD_PAGE;
            Transaction.DCTransfer.command.ui32TxLength = 2;

            am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_WRITE_DEFAULT;
            lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
            // DBI-16: high byte is a parameter when the Generic write byte count is even.
            lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
            Transaction.DDBLutCfg.sCmdAttr = lutAttr;
            Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
        }
        else
        {
            ui8CmdBuffer[0] = 0x80;
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_CMD_PAGE;
            Transaction.DCTransfer.command.ui32TxLength = 1;
        }
        ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            return ui32Status;
        }

        if (g_sDispCfg.eInterface == DISP_IF_DSI)
        {
            ui8CmdBuffer[0] = RM69330_MIPI_SET_0x00;
            ui8CmdBuffer[1] = 0xF8;
#if DSI_APB_PACKET_INTERFACE
            Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[1] << 8 | ui8CmdBuffer[0];
            Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
#else
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_0x00;
            Transaction.DCTransfer.command.ui32TxLength = 2;

            am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_WRITE_DEFAULT;
            lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
            // DBI-16: high byte is a parameter when the Generic write byte count is even.
            lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
            Transaction.DDBLutCfg.sCmdAttr = lutAttr;
            Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
        }
        else
        {
            ui8CmdBuffer[0] = 0xF8;
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_0x00;
            Transaction.DCTransfer.command.ui32TxLength = 1;
        }
        ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            return ui32Status;
        }


        if (g_sDispCfg.eInterface == DISP_IF_DSI)
        {
            ui8CmdBuffer[0] = RM69330_MIPI_SET_CMD_PAGE;
            ui8CmdBuffer[1] = 0x0;
#if DSI_APB_PACKET_INTERFACE
            Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[1] << 8 | ui8CmdBuffer[0];
            Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS;
#else
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_CMD_PAGE;
            Transaction.DCTransfer.command.ui32TxLength = 2;

            am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_WRITE_DEFAULT;
            lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
            // DBI-16: high byte is a parameter when the Generic write byte count is even.
            lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
            Transaction.DDBLutCfg.sCmdAttr = lutAttr;
            Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
        }
        else
        {
            ui8CmdBuffer[0] = 0x0;
            Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_CMD_PAGE;
            Transaction.DCTransfer.command.ui32TxLength = 1;
        }
        ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            return ui32Status;
        }
    }

    if ( g_sDispCfg.eInterface != DISP_IF_DSI )
    {
        //
        // Select DSPI 1P1T 2-wire mode when the BSP is configured for DSPI.
        //
        if ( g_sDispCfg.eInterface == DISP_IF_DSPI )
        {
            ui8CmdBuffer[0] = 0xA1;
        }
        else
        {
            ui8CmdBuffer[0] = 0x80;
        }
        Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_DSPI_MODE;
        Transaction.DCTransfer.command.ui32TxLength = 1;
        ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            return ui32Status;
        }

        Transaction.DCTransfer.ui8Command = RM69330_MIPI_SET_WR_DISPLAY_CTRL;
        ui8CmdBuffer[0] = 0x20;
        Transaction.DCTransfer.command.ui32TxLength = 1;
        ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            return ui32Status;
        }
    }

    //
    // Set panel pixel format (lower 6 bits of COLMOD).
    //
    ui8CmdBuffer[0] = (uint8_t)(pDevCfg->ui32PixelFormat & 0x3f);
    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
#if DSI_APB_PACKET_INTERFACE
        Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[0] << 8 | AM_HAL_DC_CMD_TYPE_SET_PIXEL_FORMAT;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_1_PARAMETER;
#else
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_PIXEL_FORMAT;
        Transaction.DCTransfer.command.ui32TxLength = 1;

        am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_DCS_WRITE_DEFAULT;
        lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
        // DBI-16: high byte is a parameter when the Generic write byte count is odd.
        lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = Transaction.DCTransfer.command.ui32TxLength % 2;
        Transaction.DDBLutCfg.sCmdAttr = lutAttr;
        Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
    }
    else
    {
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_PIXEL_FORMAT;
        Transaction.DCTransfer.command.ui32TxLength = 1;
    }
    ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Clear flip bits in MADCTL before further configuration.
    //
    ui8CmdBuffer[0] = 0x00;
    am_devices_display_rm69330_flip(pDevice, ui8CmdBuffer[0]);

    ui8CmdBuffer[0] = 0xFF;
    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
#if DSI_APB_PACKET_INTERFACE
        Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = ui8CmdBuffer[0] << 8 | AM_HAL_DC_CMD_TYPE_SET_DISPLAY_BRIGHTNESS;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_1_PARAMETER;
#else
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_DISPLAY_BRIGHTNESS;
        Transaction.DCTransfer.command.ui32TxLength = 1;

        am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_DCS_WRITE_DEFAULT;
        lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
        // DBI-16: high byte is a parameter when the Generic write byte count is odd.
        lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = Transaction.DCTransfer.command.ui32TxLength % 2;
        Transaction.DDBLutCfg.sCmdAttr = lutAttr;
        Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
    }
    else
    {
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_DISPLAY_BRIGHTNESS;
        Transaction.DCTransfer.command.ui32TxLength = 1;
    }
    ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Exit sleep mode, turn the display on, then set the update window.
    //
    ui8CmdBuffer[0] = 0x0;
    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
#if DSI_APB_PACKET_INTERFACE
        Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = AM_HAL_DC_CMD_TYPE_EXIT_SLEEP_MODE;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_0_PARAMETER;
#else
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_EXIT_SLEEP_MODE;
        Transaction.DCTransfer.command.ui32TxLength = 0;

        am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_DCS_WRITE_DEFAULT;
        lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
        // DBI-16: high byte is a parameter when the Generic write byte count is odd.
        lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = Transaction.DCTransfer.command.ui32TxLength % 2;
        Transaction.DDBLutCfg.sCmdAttr = lutAttr;
        Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
    }
    else
    {
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_EXIT_SLEEP_MODE;
        Transaction.DCTransfer.command.ui32TxLength = 0;
    }
    ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(130);

    ui8CmdBuffer[0] = 0x0;
    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
#if DSI_APB_PACKET_INTERFACE
        Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = AM_HAL_DC_CMD_TYPE_SET_DISPLAY_ON;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_0_PARAMETER;
#else
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_DISPLAY_ON;
        Transaction.DCTransfer.command.ui32TxLength = 0;

        am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_DCS_WRITE_DEFAULT;
        lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
        // DBI-16: high byte is a parameter when the Generic write byte count is odd.
        lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = Transaction.DCTransfer.command.ui32TxLength % 2;
        Transaction.DDBLutCfg.sCmdAttr = lutAttr;
        Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
    }
    else
    {
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_DISPLAY_ON;
        Transaction.DCTransfer.command.ui32TxLength = 0;
    }
    ui32Status = blocking_transfer((void *)pDevice, &Transaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }
    DELAY(200);

    am_devices_display_rm69330_set_region(pDevice, pDevCfg->ui16ResX,
                                          pDevCfg->ui16ResY, pDevCfg->ui16MinX,
                                          pDevCfg->ui16MinY);

    pDevice->ui32Module = ui32Module;
    pDevice->bOccupied = true;
    *ppHandle = (void *)pDevice;

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Program TE scanline (MIPI set_tear_scanline).
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_set_scanline(void *pHandle, uint16_t ui16ScanLine,
                                        uint16_t ui16ResY)
{
    uint8_t ui8CmdBuffer[4];
    am_devices_transfer_t Transaction = {0};

    Transaction.DCTransfer.command.eDirection = AM_HAL_DC_WRITE;
    Transaction.DCTransfer.command.pui8TxBuf = ui8CmdBuffer;

    Transaction.DSITransfer.ui32PktControl_b.ui32PktHSLPMode = DSI_IN_HS_MODE;
    Transaction.DSITransfer.pui32TxPayload = (uint32_t *)ui8CmdBuffer;

    if ( ui16ScanLine > ui16ResY )
    {
        return AM_DEVICES_DISPLAY_STATUS_OUT_OF_RANGE;
    }

    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
        ui8CmdBuffer[0] = AM_HAL_DC_CMD_TYPE_SET_TEAR_SCANLINE;
        ui8CmdBuffer[1] = ui16ScanLine >> 8;
        ui8CmdBuffer[2] = ui16ScanLine & 0x00FF;
#if DSI_APB_PACKET_INTERFACE
        Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = 3;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_GENERIC_LONG_WRITE;
#else
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_TEAR_SCANLINE;
        Transaction.DCTransfer.command.ui32TxLength = 3;

        am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_WRITE_DEFAULT;
        lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
        // DBI-16: high byte is a parameter when the Generic write byte count is odd.
        lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = (Transaction.DCTransfer.command.ui32TxLength + 1) % 2;
        Transaction.DDBLutCfg.sCmdAttr = lutAttr;
        Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
    }
    else
    {
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_TEAR_SCANLINE;
        ui8CmdBuffer[0] = ui16ScanLine >> 8;
        ui8CmdBuffer[1] = ui16ScanLine & 0x00FF;
        Transaction.DCTransfer.command.ui32TxLength = 2;
    }
    uint32_t ui32Status = blocking_transfer(pHandle, &Transaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set column/page address window for subsequent frame updates.
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_set_region(void *pHandle, uint16_t ui16ResX,
                                      uint16_t ui16ResY, uint16_t ui16MinX,
                                      uint16_t ui16MinY)
{
    uint8_t ui8CmdBuffer[8];
    uint16_t ui16MaxX, ui16MaxY;
    am_devices_transfer_t Transaction = {0};

    Transaction.DCTransfer.command.eDirection = AM_HAL_DC_WRITE;
    Transaction.DCTransfer.command.pui8TxBuf = ui8CmdBuffer;

    Transaction.DSITransfer.ui32PktControl_b.ui32PktHSLPMode = DSI_IN_HS_MODE;
    Transaction.DSITransfer.pui32TxPayload = (uint32_t *)ui8CmdBuffer;

    //
    // Offset compensation
    //
    ui16MinX += g_sDispCfg.ui16Offset;

    ui16MaxX = ui16MinX + ui16ResX - 1;
    ui16MaxY = ui16MinY + ui16ResY - 1;
    //
    // Set MIPI Panel region to be updated
    //
    if (g_sDispCfg.eInterface == DISP_IF_DSI)
    {
#if DSI_APB_PACKET_INTERFACE
        ui8CmdBuffer[0] = AM_HAL_DC_CMD_TYPE_SET_COLUMN_ADDRESS;
        ui8CmdBuffer[1] = (uint8_t)(ui16MinX >> 8U);
        ui8CmdBuffer[2] = (uint8_t)(ui16MinX  & 0xFFU);
        ui8CmdBuffer[3] = (uint8_t)(ui16MaxX >> 8U);
        ui8CmdBuffer[4] = (uint8_t)(ui16MaxX  & 0xFFU);
        Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = 5;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_LONG_WRITE;
#else
        ui8CmdBuffer[0] = (uint8_t)(ui16MinX >> 8U);
        ui8CmdBuffer[1] = (uint8_t)(ui16MinX  & 0xFFU);
        ui8CmdBuffer[2] = (uint8_t)(ui16MaxX >> 8U);
        ui8CmdBuffer[3] = (uint8_t)(ui16MaxX  & 0xFFU);
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_COLUMN_ADDRESS;
        ransaction.DCTransfer.command.ui32TxLength = 4;

        am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_DCS_WRITE_DEFAULT;
        lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
        // DBI-16: high byte is a parameter when the Generic write byte count is odd.
        lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = Transaction.DCTransfer.command.ui32TxLength % 2;
        Transaction.DDBLutCfg.sCmdAttr = lutAttr;
        Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
    }
    else
    {
        ui8CmdBuffer[0] = (uint8_t)(ui16MinX >> 8U);
        ui8CmdBuffer[1] = (uint8_t)(ui16MinX  & 0xFFU);
        ui8CmdBuffer[2] = (uint8_t)(ui16MaxX >> 8U);
        ui8CmdBuffer[3] = (uint8_t)(ui16MaxX  & 0xFFU);
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_COLUMN_ADDRESS;
        Transaction.DCTransfer.command.ui32TxLength = 4;
    }
    uint32_t ui32Status = blocking_transfer(pHandle, &Transaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    if (g_sDispCfg.eInterface == DISP_IF_DSI && DSI_APB_PACKET_INTERFACE)
    {
#if DSI_APB_PACKET_INTERFACE
        ui8CmdBuffer[0] = AM_HAL_DC_CMD_TYPE_SET_PAGE_ADDRESS;
        ui8CmdBuffer[1] = (uint8_t)(ui16MinY >> 8U);
        ui8CmdBuffer[2] = (uint8_t)(ui16MinY  & 0xFFU);
        ui8CmdBuffer[3] = (uint8_t)(ui16MaxY >> 8U);
        ui8CmdBuffer[4] = (uint8_t)(ui16MaxY  & 0xFFU);
        Transaction.DSITransfer.ui32PktControl_b.ui32PktWC = 5;
        Transaction.DSITransfer.ui32PktControl_b.ui32PktDT = AM_HAL_DSI_DT_DCS_LONG_WRITE;
#else
        ui8CmdBuffer[0] = (uint8_t)(ui16MinY >> 8U);
        ui8CmdBuffer[1] = (uint8_t)(ui16MinY  & 0xFFU);
        ui8CmdBuffer[2] = (uint8_t)(ui16MaxY >> 8U);
        ui8CmdBuffer[3] = (uint8_t)(ui16MaxY  & 0xFFU);
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_PAGE_ADDRESS;
        Transaction.DCTransfer.command.ui32TxLength = 4;

        am_hal_ddb_lut_cmd_attr_t lutAttr = AM_HAL_DDB_LUT_CMD_ATTR_DCS_WRITE_DEFAULT;
        lutAttr.ui32LUTWR0_b.ui32ModePolicy = (DSI_IN_HS_MODE == 1) ? AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS : AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP;
        // DBI-16: high byte is a parameter when the Generic write byte count is odd.
        lutAttr.ui32LUTWR0_b.ui32Dbi16CmdHiIsParam = Transaction.DCTransfer.command.ui32TxLength % 2;
        Transaction.DDBLutCfg.sCmdAttr = lutAttr;
        Transaction.DDBLutCfg.ui32LUTWRAddr = Transaction.DCTransfer.ui8Command;
#endif
    }
    else
    {
        ui8CmdBuffer[0] = (uint8_t)(ui16MinY >> 8U);
        ui8CmdBuffer[1] = (uint8_t)(ui16MinY  & 0xFFU);
        ui8CmdBuffer[2] = (uint8_t)(ui16MaxY >> 8U);
        ui8CmdBuffer[3] = (uint8_t)(ui16MaxY  & 0xFFU);
        Transaction.DCTransfer.ui8Command = AM_HAL_DC_CMD_TYPE_SET_PAGE_ADDRESS;
        Transaction.DCTransfer.command.ui32TxLength = 4;
    }
    ui32Status = blocking_transfer(pHandle, &Transaction, 100);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
