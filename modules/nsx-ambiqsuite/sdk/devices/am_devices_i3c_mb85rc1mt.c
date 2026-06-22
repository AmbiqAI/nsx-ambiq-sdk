//*****************************************************************************
//
//! @file am_devices_i3c_mb85rc1mt.c
//!
//! @brief Fujitsu MB85RC1MT 128K FRAM Memory Driver with I3C Interface.
//!
//! @addtogroup devices_i3c_mb85rc1mt MB85RC1MT I3C FRAM Memory Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a comprehensive hardware abstraction layer
//!          for the Fujitsu MB85RC1MT ferroelectric random access memory (FRAM).
//!          The driver enables high-speed, non-volatile memory operations through
//!          the I3C interface. FRAM technology offers virtually unlimited write
//!          endurance, instant write capability, and low power consumption,
//!          making it ideal for data logging, configuration storage, and
//!          applications requiring frequent write operations.
//!
//! @section devices_i3c_mb85rc1mt_features Key Features
//!
//! 1. @b 128K @b FRAM @b Memory: Non-volatile ferroelectric memory with instant write.
//! 2. @b Unlimited @b Write @b Endurance: No wear-out mechanism like flash memory.
//! 3. @b I3C @b Interface: High-speed digital communication with advanced features.
//! 4. @b Fast @b Write @b Operations: No write delays or page programming required.
//! 5. @b Low @b Power @b Consumption: Ultra-low power operation for battery applications.
//! 6. @b Blocking @b and @b Non-blocking @b Operations: Flexible data transfer modes.
//! 7. @b DMA @b Support: Efficient large data transfers with timeout mechanisms.
//! 8. @b Device @b ID @b Support: Built-in device identification and verification.
//!
//! @section devices_i3c_mb85rc1mt_functionality Functionality
//!
//! - Initialize and configure MB85RC1MT FRAM device via I3C interface
//! - Perform blocking read and write operations for immediate data access
//! - Support non-blocking read and write operations with callback mechanisms
//! - Handle device identification and verification
//! - Provide flexible addressing for 128K memory space (0x0000 to 0x1FFFF)
//! - Support both single-byte and multi-byte data transfers
//! - Manage DMA operations with timeout and completion callbacks
//! - Handle I3C-specific communication protocols and error conditions
//!
//! @section devices_i3c_mb85rc1mt_usage Usage
//!
//! 1. Initialize I3C host interface and configure I3C module
//! 2. Initialize MB85RC1MT device with am_devices_i3c_mb85rc1mt_init()
//! 3. Verify device presence with am_devices_i3c_mb85rc1mt_read_id()
//! 4. Perform blocking writes with am_devices_i3c_mb85rc1mt_blocking_write()
//! 5. Perform blocking reads with am_devices_i3c_mb85rc1mt_blocking_read()
//! 6. Use non-blocking operations for high-performance applications
//! 7. Monitor completion status through callback functions or polling
//! 8. Terminate device with am_devices_i3c_mb85rc1mt_term() when finished
//!
//! @section devices_i3c_mb85rc1mt_configuration Configuration
//!
//! - Set up I3C interface parameters and timing
//! - Configure FRAM memory addressing (16-bit addresses for 128K space)
//! - Set up DMA parameters for efficient large data transfers
//! - Configure timeout mechanisms for reliable operation
//! - Set up callback functions for non-blocking operations
//! - Configure error handling and retry mechanisms
//! - Optimize transfer sizes for best performance
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.06.18 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#include <string.h>
#include <stdlib.h>
#include "am_mcu_apollo.h"
#include "am_devices_i3c_mb85rc1mt.h"
#include "am_util.h"

typedef struct
{
    uint32_t                ui32Module;
    void                    *pI3CHandle;
    bool                    bOccupied;
    uint8_t                 ui8StaticAddr;
    am_devices_i3c_mb85rc1mt_config_t sI3cmb85rc1mtCfg;
} am_devices_i3c_mb85rc1mt_t;

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************

am_devices_i3c_mb85rc1mt_t   gAmI3cMb85rc1mt[AM_DEVICES_MB85RC1MT_MAX_DEVICE_NUM];

//*****************************************************************************
//
//  MB85RC1MT device driver default callback to wait DMA complete
//
//*****************************************************************************
static uint32_t
am_devices_i3c_mb85rc1mt_wait_dma_done(bool *pDMAComplete)
{
    uint32_t ui32Timeout = AM_DEVICES_MB85RC1MT_DMA_TIMEOUT_CNT;
    //
    // Wait until DMA transfer done
    //
    while (!(*(volatile bool *)pDMAComplete))
    {
        if (ui32Timeout == 0)
        {
            break;
        }
        am_util_delay_us(AM_DEVICES_MB85RC1MT_DMA_DELAY_US);
        am_util_debug_printf("waiting non-blocking complete\n");
        ui32Timeout--;
    }

    if ( ui32Timeout == 0 )
    {
        am_util_stdio_printf("Non-blocking timeout\n");
        return AM_DEVICES_MB85RC1MT_STATUS_TIMEOUT;
    }

    return AM_DEVICES_MB85RC1MT_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Programs data into the given sub address of device.
//
//*****************************************************************************
uint32_t
am_devices_i3c_mb85rc1mt_blocking_write(void *pHandle, uint16_t ui16SubAddr,
                                        uint32_t *pData, uint32_t ui32NumBytes)
{
    uint32_t ui32Status = AM_DEVICES_MB85RC1MT_STATUS_SUCCESS;
    am_hal_i3c_transfer_t Transaction;
    am_devices_i3c_mb85rc1mt_t *pDevHandle = (am_devices_i3c_mb85rc1mt_t *)pHandle;
    am_devices_i3c_mb85rc1mt_config_t *pDeviceCfg = &pDevHandle->sI3cmb85rc1mtCfg;

    if ( !pHandle )
    {
        return AM_DEVICES_MB85RC1MT_STATUS_INVALID_HANDLE;
    }

    //
    // Save target reg address to buffer
    //
    uint8_t *pui8Buffer = (uint8_t *)pData;
    pui8Buffer[0] = (ui16SubAddr >> 8) & 0xFF;
    pui8Buffer[1] = ui16SubAddr & 0xFF;

    Transaction.Device.ui8StaticAddr  = pDevHandle->ui8StaticAddr;
    Transaction.Device.eDeviceType    = AM_HAL_I3C_DEVICE_I2C;

    //
    // Create the transaction
    //
    Transaction.ui32NumBytes  = ui32NumBytes + 2;
    Transaction.eSpeedMode    = pDeviceCfg->eSpeedMode;
    Transaction.eDirection    = AM_HAL_I3C_DIR_WRITE;
    Transaction.pui32TxBuffer = pData;

    Transaction.bComboXfer     = false;
    Transaction.bCombo16bitOfs = false;
    Transaction.ui16ComboOfs   = 0;

    //
    // Execute legacy i2c transction over I3C.
    //
    ui32Status = am_hal_i3c_blocking_transfer(pDevHandle->pI3CHandle, &Transaction);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        am_util_stdio_printf("I3C leagcy I2C blocking write failed:0x%x\n", ui32Status);
        return ui32Status;
    }

    return ui32Status;
}

//*****************************************************************************
//
//  Reads the contents based on the given sub address of device.
//
//*****************************************************************************
uint32_t
am_devices_i3c_mb85rc1mt_blocking_read(void *pHandle, uint16_t ui16SubAddr,
                                       uint32_t *pData, uint32_t ui32NumBytes)
{
    uint32_t ui32Status = AM_DEVICES_MB85RC1MT_STATUS_SUCCESS;
    am_hal_i3c_transfer_t Transaction;
    am_devices_i3c_mb85rc1mt_t *pDevHandle = (am_devices_i3c_mb85rc1mt_t *)pHandle;
    am_devices_i3c_mb85rc1mt_config_t *pDeviceCfg = &pDevHandle->sI3cmb85rc1mtCfg;

    if ( !pHandle )
    {
        return AM_DEVICES_MB85RC1MT_STATUS_INVALID_HANDLE;
    }

    Transaction.Device.ui8StaticAddr  = pDevHandle->ui8StaticAddr;
    Transaction.Device.eDeviceType    = AM_HAL_I3C_DEVICE_I2C;
    Transaction.eSpeedMode    = pDeviceCfg->eSpeedMode;
    Transaction.bComboXfer     = true;
    Transaction.bCombo16bitOfs = true;
    Transaction.ui16ComboOfs   = ui16SubAddr;

    //
    // Execute Read Transaction
    //
    Transaction.ui32NumBytes  = ui32NumBytes;
    Transaction.eDirection    = AM_HAL_I3C_DIR_READ;
    Transaction.pui32RxBuffer = pData;

    ui32Status = am_hal_i3c_blocking_transfer(pDevHandle->pI3CHandle, &Transaction);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        am_util_stdio_printf("I3C leagcy I2C blocking read failed:0x%x\n", ui32Status);
        return ui32Status;
    }

    return ui32Status;
}

//*****************************************************************************
//
//  Programs data into the given sub address of device.
//
//*****************************************************************************
uint32_t
am_devices_i3c_mb85rc1mt_nonblocking_write(void *pHandle, uint32_t ui32SubAddr,
                                           uint32_t *pData, uint32_t ui32NumBytes)
{
    uint32_t ui32Status = AM_DEVICES_MB85RC1MT_STATUS_SUCCESS;
    am_hal_i3c_transfer_t      Transaction;
    am_devices_i3c_mb85rc1mt_t *pDevHandle = (am_devices_i3c_mb85rc1mt_t *)pHandle;
    am_devices_i3c_mb85rc1mt_config_t *pDeviceCfg = &pDevHandle->sI3cmb85rc1mtCfg;
    uint32_t ui32BufAddr = 0;
    am_hal_cachectrl_range_t sRange;

    if ( !pHandle )
    {
        return AM_DEVICES_MB85RC1MT_STATUS_INVALID_HANDLE;
    }

    //
    // Save target reg address to write buffer
    //
    uint8_t *pui8Buffer = (uint8_t *)pData;
    pui8Buffer[0] = (ui32SubAddr >> 8) & 0xFF;
    pui8Buffer[1] = ui32SubAddr & 0xFF;

    //
    // Make sure DMA TX buffer do cache cleaning
    //
    ui32BufAddr = (uint32_t)pData;
    if ( (ui32BufAddr >= SSRAM_BASEADDR) )
    {
        sRange.ui32StartAddr = (uint32_t)pData;
        sRange.ui32Size = ui32NumBytes + 2;
        am_hal_cachectrl_dcache_clean(&sRange);
    }

    //
    // Create the transaction
    //
    Transaction.Device.ui8StaticAddr  = pDevHandle->ui8StaticAddr;
    Transaction.Device.eDeviceType    = AM_HAL_I3C_DEVICE_I2C;

    Transaction.ui32NumBytes  = ui32NumBytes + 2;
    Transaction.eSpeedMode    = pDeviceCfg->eSpeedMode;
    Transaction.eDirection    = AM_HAL_I3C_DIR_WRITE;
    Transaction.pui32TxBuffer = (uint32_t *)pData;

    Transaction.bComboXfer     = false;
    Transaction.bCombo16bitOfs = false;
    Transaction.ui16ComboOfs   = ui32SubAddr;

    //
    // Execute DMA Write Transaction
    //
    Transaction.pui32CmdRingBuf     = pDeviceCfg->sI3cDmaCfg.pCmdRingBuf;
    Transaction.ui32CmdRingBufLen   = pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen;
    Transaction.pui32RespRingBuf    = pDeviceCfg->sI3cDmaCfg.pRespRingBuf;
    Transaction.ui32RespRingBufLen  = pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen;
    *(volatile bool *)pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;

    ui32Status = am_hal_i3c_nonblocking_transfer(pDevHandle->pI3CHandle, &Transaction);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        am_util_stdio_printf("I3C leagcy I2C non-blcking write failed:0x%x\n", ui32Status);
        return ui32Status;
    }

    //
    // Wait until DMA transfer done
    //
    return pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback(pDeviceCfg->sI3cDmaCfg.pDMAComplete);
}

//*****************************************************************************
//
//  Reads the contents based on the given sub address of device.
//
//*****************************************************************************
uint32_t
am_devices_i3c_mb85rc1mt_nonblocking_read(void *pHandle, uint32_t ui32SubAddr,
                                          uint32_t *pData, uint32_t ui32NumBytes)
{
    uint32_t ui32Status = AM_DEVICES_MB85RC1MT_STATUS_SUCCESS;
    am_hal_i3c_transfer_t      Transaction;
    am_devices_i3c_mb85rc1mt_t *pDevHandle = (am_devices_i3c_mb85rc1mt_t *)pHandle;
    am_devices_i3c_mb85rc1mt_config_t *pDeviceCfg = &pDevHandle->sI3cmb85rc1mtCfg;

    if ( !pHandle )
    {
        return AM_DEVICES_MB85RC1MT_STATUS_INVALID_HANDLE;
    }

    Transaction.Device.ui8StaticAddr  = pDevHandle->ui8StaticAddr;
    Transaction.Device.eDeviceType    = AM_HAL_I3C_DEVICE_I2C;
    Transaction.eSpeedMode    = pDeviceCfg->eSpeedMode;
    Transaction.bComboXfer     = true;
    Transaction.bCombo16bitOfs = true;
    Transaction.ui16ComboOfs   = ui32SubAddr;

    //
    // Execute Read Transaction
    //
    Transaction.ui32NumBytes  = ui32NumBytes;
    Transaction.eDirection    = AM_HAL_I3C_DIR_READ;
    Transaction.pui32RxBuffer = pData;

    Transaction.pui32CmdRingBuf     = pDeviceCfg->sI3cDmaCfg.pCmdRingBuf;
    Transaction.ui32CmdRingBufLen   = pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen;
    Transaction.pui32RespRingBuf    = pDeviceCfg->sI3cDmaCfg.pRespRingBuf;
    Transaction.ui32RespRingBufLen  = pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen;
    *(volatile bool *)pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;

    ui32Status = am_hal_i3c_nonblocking_transfer(pDevHandle->pI3CHandle, &Transaction);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        am_util_stdio_printf("I3C leagcy I2C non-blcking read failed:0x%x\n", ui32Status);
        return ui32Status;
    }

    //
    // Wait until DMA transfer done
    //
    return pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback(pDeviceCfg->sI3cDmaCfg.pDMAComplete);
}

//*****************************************************************************
//
//  Reads the device id of the device.
//
//*****************************************************************************
static uint32_t
am_devices_i3c_mb85rc1mt_read_id(void *pHandle, uint32_t *pDeviceID)
{
    uint32_t ui32Status = AM_DEVICES_MB85RC1MT_STATUS_SUCCESS;
    am_hal_i3c_transfer_t Transaction;
    am_devices_i3c_mb85rc1mt_t *pDevHandle = (am_devices_i3c_mb85rc1mt_t *)pHandle;
    am_devices_i3c_mb85rc1mt_config_t *pDeviceCfg = &pDevHandle->sI3cmb85rc1mtCfg;

    if ( !pHandle )
    {
        return AM_DEVICES_MB85RC1MT_STATUS_INVALID_HANDLE;
    }

    Transaction.Device.ui8StaticAddr  = AM_DEVICES_MB85RC1MT_SLAVE_ID;
    Transaction.Device.eDeviceType    = AM_HAL_I3C_DEVICE_I2C;
    Transaction.eSpeedMode            = pDeviceCfg->eSpeedMode;

    Transaction.bComboXfer     = true;
    Transaction.bCombo16bitOfs = false;
    Transaction.ui16ComboOfs   = AM_DEVICES_MB85RC1MT_DEVICEID_ADDR;

    //
    // Execute Read Transaction
    //
    Transaction.ui32NumBytes  = 3;
    Transaction.eDirection    = AM_HAL_I3C_DIR_READ;
    Transaction.pui32RxBuffer = pDeviceID;

    if ( pDeviceCfg->eTransferMode == AM_HAL_I3C_XFER_DMA)
    {
        Transaction.pui32CmdRingBuf     = pDeviceCfg->sI3cDmaCfg.pCmdRingBuf;
        Transaction.ui32CmdRingBufLen   = pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen;
        Transaction.pui32RespRingBuf    = pDeviceCfg->sI3cDmaCfg.pRespRingBuf;
        Transaction.ui32RespRingBufLen  = pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen;
        *(volatile bool *)pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;

        ui32Status = am_hal_i3c_nonblocking_transfer(pDevHandle->pI3CHandle, &Transaction);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            am_util_stdio_printf("I3C leagcy I2C non-blcking read failed:0x%x\n", ui32Status);
            return ui32Status;
        }

        //
        // Wait until DMA transfer done
        //
        return pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback(pDeviceCfg->sI3cDmaCfg.pDMAComplete);
    }
    else
    {
        ui32Status = am_hal_i3c_blocking_transfer(pDevHandle->pI3CHandle, &Transaction);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            am_util_stdio_printf("I3C leagcy I2C blocking read failed:0x%x\n", ui32Status);
            return ui32Status;
        }
    }

    return ui32Status;
}

uint32_t
am_devices_i3c_mb85rc1mt_init(uint32_t ui32Module,
                              void *pI3cHandle, void **ppDevHandle,
                              am_devices_i3c_mb85rc1mt_config_t *pDevConfig)
{
    uint32_t ui32Index  = 0;
    uint32_t ui32DeviceId = 0;
    uint32_t ui32Status = AM_DEVICES_MB85RC1MT_STATUS_SUCCESS;

    am_devices_i3c_mb85rc1mt_config_t *pDeviceCfg;
    am_devices_i3c_mb85rc1mt_t *pDevHandle;

    //
    // Allocate a vacant device handle
    //
    for ( ui32Index = 0; ui32Index < AM_DEVICES_MB85RC1MT_MAX_DEVICE_NUM; ui32Index++ )
    {
        if ( gAmI3cMb85rc1mt[ui32Index].bOccupied == false )
        {
            pDevHandle = &gAmI3cMb85rc1mt[ui32Index];
            pDeviceCfg = &gAmI3cMb85rc1mt[ui32Index].sI3cmb85rc1mtCfg;
            break;
        }
    }

    if ( ui32Index == AM_DEVICES_MB85RC1MT_MAX_DEVICE_NUM )
    {
        return AM_DEVICES_MB85RC1MT_STATUS_OUT_OF_RANGE;
    }

    if ( (ui32Module > AM_REG_I3C_NUM_MODULES) || (pDevConfig == NULL) )
    {
        return AM_DEVICES_MB85RC1MT_STATUS_INVALID_HANDLE;
    }

    pDeviceCfg->eTransferMode       = pDevConfig->eTransferMode;
    pDeviceCfg->eSpeedMode          = pDevConfig->eSpeedMode;
    pDeviceCfg->ui8DynamicAddr      = 0x0;

    pDevHandle->ui8StaticAddr = AM_DEVICES_MB85RC1MT_STATIC_ADDRESS;
    pDevHandle->bOccupied = true;
    pDevHandle->ui32Module = ui32Module;
    pDevHandle->pI3CHandle = pI3cHandle;
    *ppDevHandle = pDevHandle;

    if ( pDevConfig->eTransferMode == AM_HAL_I3C_XFER_DMA )
    {
        if ( pDevConfig->sI3cDmaCfg.pDMAComplete == NULL )
        {
            return AM_DEVICES_MB85RC1MT_STATUS_INVALID_ARG;
        }

        if ( pDevConfig->sI3cDmaCfg.pfnI3cTimeoutCallback == NULL )
        {
            pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback = am_devices_i3c_mb85rc1mt_wait_dma_done;
        }
        else
        {
            pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback = pDevConfig->sI3cDmaCfg.pfnI3cTimeoutCallback;
        }

        pDeviceCfg->sI3cDmaCfg.pDMAComplete = pDevConfig->sI3cDmaCfg.pDMAComplete;

        pDeviceCfg->sI3cDmaCfg.pCmdRingBuf        = pDevConfig->sI3cDmaCfg.pCmdRingBuf;
        pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen  = pDevConfig->sI3cDmaCfg.ui32CmdRingBufLen;
        pDeviceCfg->sI3cDmaCfg.pRespRingBuf       = pDevConfig->sI3cDmaCfg.pRespRingBuf;
        pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen = pDevConfig->sI3cDmaCfg.ui32RespRingBufLen;

    }


    //
    // Set legacy I2C speed
    //
    if ( pDeviceCfg->eSpeedMode > AM_HAL_I3C_I2C_MODE1 )
    {
        uint8_t ui8LowCnt, ui8HighCnt;

        ui8LowCnt  = AM_DEVICES_MB85RC1MT_SCL_LOW_CNT(pDeviceCfg->eSpeedMode);
        ui8HighCnt = AM_DEVICES_MB85RC1MT_SCL_HIGH_CNT(pDeviceCfg->eSpeedMode);
        ui32Status = am_hal_i3c_set_i2c_speed(pDevHandle->pI3CHandle, pDeviceCfg->eSpeedMode, ui8LowCnt, ui8HighCnt);
        if ( ui32Status != AM_HAL_STATUS_SUCCESS )
        {
            am_util_stdio_printf("Leagcy I2C set speed fail :0x%x\n", ui32Status);
            return AM_DEVICES_MB85RC1MT_STATUS_ERROR;
        }
    }

    //
    // Read device ID
    //
    ui32Status = am_devices_i3c_mb85rc1mt_read_id(pDevHandle, &ui32DeviceId);
    if ( AM_DEVICES_MB85RC1MT_STATUS_SUCCESS != ui32Status )
    {
        am_util_stdio_printf("Failed to read device id!\n");
        return ui32Status;
    }

    if ( (ui32DeviceId & 0xFFFFFF) != AM_DEVICES_MB85RC1MT_DEVICE_ID)
    {
        am_util_stdio_printf("Device id mismatch, 0x%x\n", ui32DeviceId);
        return AM_DEVICES_MB85RC1MT_STATUS_ERROR;
    }

    return AM_DEVICES_MB85RC1MT_STATUS_SUCCESS;
}

uint32_t
am_devices_i3c_mb85rc1mt_term(void *pHandle)
{
    am_devices_i3c_mb85rc1mt_t *pDevHandle = (am_devices_i3c_mb85rc1mt_t *)pHandle;

    if ( pDevHandle->ui32Module > AM_REG_I3C_NUM_MODULES )
    {
        return AM_DEVICES_MB85RC1MT_STATUS_OUT_OF_RANGE;
    }

    //
    // Free this device handle
    //
    pDevHandle->bOccupied = false;

    //
    // Return the status.
    //
    return AM_DEVICES_MB85RC1MT_STATUS_SUCCESS;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
