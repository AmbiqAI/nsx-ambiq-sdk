//*****************************************************************************
//
//! @file am_devices_display_rm69330.c
//!
//! @brief Driver for Raydium RM69330 (and compatible) panels over DC xSPI.
//!
//! Sends MIPI DCS commands through the display controller to configure tearing,
//! pixel format, DSPI mode, brightness, and the active display window.
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
// This is part of revision stable-2026.06.17 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include "am_devices_display_rm69330.h"
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

typedef struct
{
    uint32_t                    ui32Module;
    void                        *pDCHandle;
    bool                        bOccupied;
} am_devices_rm69330_t;

am_devices_rm69330_t gRm69330[AM_REG_DC_NUM_MODULES];

//*****************************************************************************
//
// Reset the display panel
//
//*****************************************************************************
void
am_devices_display_rm69330_hardware_reset(void *pHandle)
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
// Fill in default NemaDC xSPI configuration for the RM69330 panel.
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_get_parameter(void *pHandle, am_hal_dc_config_t *pDCCfg)
{
    //
    // Default to QSPI; callers may override eInterface before am_hal_dc_config().
    //
    pDCCfg->eInterface = AM_HAL_DC_IF_QSPI;
    pDCCfg->bTEEnable = false;
    pDCCfg->ui32DstFormat = AM_HAL_DC_FORMAT_RGB888;

    //
    // Use BSP display resolution and 1:1 panel mapping.
    //
    pDCCfg->sSPICfg.sTiming.resx = g_sDispCfg.ui16ResX;
    pDCCfg->sSPICfg.sTiming.resy = g_sDispCfg.ui16ResY;
    pDCCfg->sSPICfg.sTiming.fpx = 1;
    pDCCfg->sSPICfg.sTiming.fpy = 1;
    pDCCfg->sSPICfg.sTiming.bpx = 1;
    pDCCfg->sSPICfg.sTiming.bpy = 1;
    pDCCfg->sSPICfg.sTiming.blx = 1;
    pDCCfg->sSPICfg.sTiming.bly = 1;

    pDCCfg->sSPICfg.ui32PolPha = MIPICFG_SPI_CPOL | MIPICFG_SPI_CPHA;
    pDCCfg->sSPICfg.bDDREnable = false;
    pDCCfg->sSPICfg.fTargetClkWrite = 48.0;
    pDCCfg->sSPICfg.fTargetClkRead = 10.0;

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}
//*****************************************************************************
//
// Flip the image along with x or y or x/y-axis
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_flip(void *pHandle, uint8_t ui8FlipXY)
{
    am_devices_rm69330_t *pDevice = (am_devices_rm69330_t *)pHandle;
    uint8_t ui8CmdBuffer[4];
    ui8CmdBuffer[0] = ui8FlipXY;
    am_hal_dc_transfer_t Transaction;

    Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_SET_ADDRESS_MODE;
    Transaction.command.eDirection = AM_HAL_DC_WRITE;
    Transaction.command.pui8TxBuf = ui8CmdBuffer;
    Transaction.command.ui32TxLength = 1;

    uint32_t ui32Status = am_hal_dc_blocking_transfer(pDevice->pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Initialize RM69330 with SPI4/DSPI/QSPI/MIPI-DSI interfaces.
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_init(uint32_t ui32Module,
                                am_devices_dc_config_t *pDevCfg,
                                void **ppHandle, void *pDCHandle)
{
    uint8_t ui8CmdBuffer[4];
    am_hal_dc_transfer_t Transaction;

    if(ui32Module > AM_REG_DC_NUM_MODULES)
    {
        return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    am_devices_rm69330_t *pDevice = &gRm69330[ui32Module];
    pDevice->pDCHandle = pDCHandle;

    Transaction.command.eDirection = AM_HAL_DC_WRITE;
    Transaction.command.pui8TxBuf = ui8CmdBuffer;
    //
    // Enable/disable tearing
    //
    if ( pDevCfg->bTEEnable )
    {
        ui8CmdBuffer[0] = 0x02;
        Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_SET_TEAR_ON;
        Transaction.command.ui32TxLength = 1;
    }
    else
    {
        ui8CmdBuffer[0] = 0x0;
        Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_SET_TEAR_OFF;
        Transaction.command.ui32TxLength = 0;
    }
    uint32_t ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return ui32Status;
    }

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
    const int MIPI_set_dspi_mode = 0xc4;
    Transaction.ui8Command = MIPI_set_dspi_mode;
    Transaction.command.ui32TxLength = 0;
    ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Set panel pixel format (lower 6 bits of COLMOD).
    //
    ui8CmdBuffer[0] = (uint8_t)(pDevCfg->ui32PixelFormat & 0x3f);
    Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_SET_PIXEL_FORMAT;
    Transaction.command.ui32TxLength = 1;
    ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Clear flip bits in MADCTL before further configuration.
    //
    ui8CmdBuffer[0] = 0x00;
    am_devices_display_rm69330_flip(pDevice, ui8CmdBuffer[0]);
    DELAY(10);

    //
    // Unlock vendor CMD2 page on CO5300-compatible controllers.
    //
    if (g_sDispCfg.eIC == DISP_IC_CO5300)
    {
        const int MIPI_set_cmd_page = 0xFE;
        Transaction.ui8Command = MIPI_set_cmd_page;
        ui8CmdBuffer[0] = 0x20;
        Transaction.command.ui32TxLength = 1;
        ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
        if(ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return ui32Status;
        }

        Transaction.ui8Command = 0xF4;
        ui8CmdBuffer[0] = 0x5A;
        Transaction.command.ui32TxLength = 1;
        ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
        if(ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return ui32Status;
        }

        Transaction.ui8Command = 0xF5;
        ui8CmdBuffer[0] = 0x59;
        Transaction.command.ui32TxLength = 1;
        ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
        if(ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return ui32Status;
        }

        Transaction.ui8Command = MIPI_set_cmd_page;
        ui8CmdBuffer[0] = 0x80;
        Transaction.command.ui32TxLength = 1;
        ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
        if(ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return ui32Status;
        }

        Transaction.ui8Command = 0x00;
        ui8CmdBuffer[0] = 0xF8;
        Transaction.command.ui32TxLength = 1;
        ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
        if(ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return ui32Status;
        }

        Transaction.ui8Command = MIPI_set_cmd_page;
        ui8CmdBuffer[0] = 0x0;
        Transaction.command.ui32TxLength = 1;
        ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
        if(ui32Status != AM_HAL_STATUS_SUCCESS)
        {
            return ui32Status;
        }
    }

    const int MIPI_set_wr_display_ctrl = 0x53;
    Transaction.ui8Command = MIPI_set_wr_display_ctrl;
    ui8CmdBuffer[0] = 0x20;
    Transaction.command.ui32TxLength = 1;
    ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_SET_DISPLAY_BRIGHTNESS;
    ui8CmdBuffer[0] = 0xff;
    Transaction.command.ui32TxLength = 1;
    ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Exit sleep, turn the panel on, then program the update window.
    //
    Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_EXIT_SLEEP_MODE;
    ui8CmdBuffer[0] = 0x0;
    Transaction.command.ui32TxLength = 0;
    ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return ui32Status;
    }
    DELAY(130);

    Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_SET_DISPLAY_ON;
    ui8CmdBuffer[0] = 0x0;
    Transaction.command.ui32TxLength = 0;
    ui32Status = am_hal_dc_blocking_transfer(pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
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
// Set scanline
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_set_scanline(void *pHandle, uint16_t ui16ScanLine,
                                        uint16_t ui16ResY)
{
    am_devices_rm69330_t *pDevice = (am_devices_rm69330_t *)pHandle;
    uint8_t ui8CmdBuffer[2];
    am_hal_dc_transfer_t Transaction;

    if ( ui16ScanLine > ui16ResY )
    {
        return AM_DEVICES_DISPLAY_STATUS_OUT_OF_RANGE;
    }

    ui8CmdBuffer[0] = ui16ScanLine >> 8;
    ui8CmdBuffer[1] = ui16ScanLine & 0x00FF;

    Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_SET_TEAR_SCANLINE;
    Transaction.command.eDirection = AM_HAL_DC_WRITE;
    Transaction.command.pui8TxBuf = ui8CmdBuffer;
    Transaction.command.ui32TxLength = 2;

    uint32_t ui32Status = am_hal_dc_blocking_transfer(pDevice->pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Set display region/window
//
//*****************************************************************************
uint32_t
am_devices_display_rm69330_set_region(void *pHandle, uint16_t ui16ResX,
                                      uint16_t ui16ResY, uint16_t ui16MinX,
                                      uint16_t ui16MinY)
{
    am_devices_rm69330_t *pDevice = (am_devices_rm69330_t *)pHandle;
    uint8_t ui8CmdBuffer[4];
    uint16_t ui16MaxX, ui16MaxY;
    am_hal_dc_transfer_t Transaction;

    ui16MaxX = ui16MinX + ui16ResX - 1;
    ui16MaxY = ui16MinY + ui16ResY - 1;
    //
    // Set MIPI Panel region to be updated
    //
    ui8CmdBuffer[0] = (uint8_t)(ui16MinX >> 8U);
    ui8CmdBuffer[1] = (uint8_t)(ui16MinX  & 0xFFU);
    ui8CmdBuffer[2] = (uint8_t)(ui16MaxX >> 8U);
    ui8CmdBuffer[3] = (uint8_t)(ui16MaxX  & 0xFFU);
    Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_SET_COLUMN_ADDRESS;
    Transaction.command.eDirection = AM_HAL_DC_WRITE;
    Transaction.command.pui8TxBuf = ui8CmdBuffer;
    Transaction.command.ui32TxLength = 4;

    uint32_t ui32Status = am_hal_dc_blocking_transfer(pDevice->pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    ui8CmdBuffer[0] = (uint8_t)(ui16MinY >> 8U);
    ui8CmdBuffer[1] = (uint8_t)(ui16MinY  & 0xFFU);
    ui8CmdBuffer[2] = (uint8_t)(ui16MaxY >> 8U);
    ui8CmdBuffer[3] = (uint8_t)(ui16MaxY  & 0xFFU);

    Transaction.ui8Command = AM_HAL_DC_CMD_TYPE_SET_PAGE_ADDRESS;
    Transaction.command.eDirection = AM_HAL_DC_WRITE;
    Transaction.command.pui8TxBuf = ui8CmdBuffer;
    Transaction.command.ui32TxLength = 4;

    ui32Status = am_hal_dc_blocking_transfer(pDevice->pDCHandle, &Transaction, 100);
    if(ui32Status != AM_HAL_STATUS_SUCCESS)
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
