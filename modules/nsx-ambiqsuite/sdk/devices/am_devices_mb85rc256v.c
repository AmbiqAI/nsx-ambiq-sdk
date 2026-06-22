//*****************************************************************************
//
//! @file am_devices_mb85rc256v.c
//!
//! @brief Device driver for Fujitsu MB85RC256V peripheral.
//!
//! @addtogroup devices_mb85rc256v Fujitsu MB85RC256V Device Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a hardware abstraction layer
//!          for the MB85RC256V Fujitsu 256K I2C FRAM (Ferroelectric Random Access
//!          Memory) device with support for high-speed read/write operations and
//!          non-volatile data storage. It enables reliable memory access, device
//!          identification, and power-efficient operation for embedded applications
//!          requiring persistent memory with fast access times. The driver supports
//!          both blocking and non-blocking operations, I2C communication, and
//!          features for optimal performance across various FRAM configurations.
//!
//! @section devices_mb85rc256v_features Key Features
//!
//! 1. @b High-speed @b Read/Write: Fast memory access operations.
//! 2. @b Non-volatile @b Storage: Persistent data retention.
//! 3. @b I2C @b Communication: Standard I2C protocol support.
//! 4. @b Device @b Identification: Read device ID functionality.
//! 5. @b Power @b Efficiency: Low-power operation for embedded systems.
//!
//! @section devices_mb85rc256v_functionality Functionality
//!
//! - Initialize and deinitialize FRAM device
//! - Perform blocking and non-blocking read/write operations
//! - Read device ID and status
//! - Manage power and I2C configuration
//!
//! @section devices_mb85rc256v_usage Usage
//!
//! 1. Initialize device with am_devices_mb85rc256v_init()
//! 2. Read/write data using blocking/non-blocking APIs
//! 3. Retrieve device ID with am_devices_mb85rc256v_read_id()
//!
//! @section devices_mb85rc256v_configuration Configuration
//!
//! - Configure I2C clock frequency and buffer sizes
//! - Set up device handle and IOM configuration
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

#include "am_mcu_apollo.h"
#include "am_devices_mb85rc256v.h"
#include "am_bsp.h"

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************
typedef struct
{
    uint32_t                    ui32Module;
    void                        *pIomHandle;
    bool                        bOccupied;
} am_devices_iom_mb85rc256v_t;

am_devices_iom_mb85rc256v_t gAmMb85rc256v[AM_DEVICES_MB85RC256V_MAX_DEVICE_NUM];

am_hal_iom_config_t     g_sIomMb85rc256vCfg =
{
    .eInterfaceMode       = AM_HAL_IOM_I2C_MODE,
    .ui32ClockFreq        = AM_HAL_IOM_1MHZ,
    .eSpiMode             = AM_HAL_IOM_SPI_MODE_0,
    .ui32NBTxnBufLength   = 0,
    .pNBTxnBuf = NULL,
};

//*****************************************************************************
//
//! @brief Handle am device command write.
//!
//! Internal helper for the am device command write operation.
//!
//! @param pHandle - Pointer to the device handle.
//! @param ui8DevAddr - Address value.
//! @param ui32InstrLen - Length/count for 32 instr len.
//! @param ui64Instr - Value for 64 instr.
//! @param bCont - Boolean flag for b cont.
//! @param pData - Data buffer pointer.
//! @param ui32NumBytes - Value for 32 num bytes.
//!
//! @return Status code: `AM_DEVICES_MB85RC256V_STATUS_SUCCESS` on success;
//!         otherwise `AM_DEVICES_MB85RC256V_STATUS_*`.
//
//*****************************************************************************
static uint32_t
am_device_command_write(void *pHandle, uint8_t ui8DevAddr, uint32_t ui32InstrLen,
                        uint64_t ui64Instr, bool bCont,
                        uint32_t *pData, uint32_t ui32NumBytes)
{
    am_hal_iom_transfer_t Transaction;

    //
    // Create the transaction.
    //
    Transaction.ui32InstrLen    = ui32InstrLen;
#if (defined(AM_PART_APOLLO4B) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || \
     defined(AM_PART_APOLLO5_API) || defined(AM_PART_ATOMIQ110))
    Transaction.ui64Instr       = ui64Instr;
#else
    Transaction.ui32Instr       = (uint32_t)ui64Instr;
#endif
    Transaction.eDirection      = AM_HAL_IOM_TX;
    Transaction.ui32NumBytes    = ui32NumBytes;
    Transaction.pui32TxBuffer   = pData;
    Transaction.uPeerInfo.ui32I2CDevAddr = ui8DevAddr;
    Transaction.bContinue       = bCont;
    Transaction.ui8RepeatCount  = 0;
    Transaction.ui32PauseCondition = 0;
    Transaction.ui32StatusSetClr = 0;

    //
    // Execute the transction over IOM.
    //
    if (am_hal_iom_blocking_transfer(pHandle, &Transaction))
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }
    return AM_DEVICES_MB85RC256V_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Handle am device command read.
//!
//! Internal helper for the am device command read operation.
//!
//! @param pHandle - Pointer to the device handle.
//! @param ui8DevAddr - Address value.
//! @param ui32InstrLen - Length/count for 32 instr len.
//! @param ui64Instr - Value for 64 instr.
//! @param bCont - Boolean flag for b cont.
//! @param pData - Data buffer pointer.
//! @param ui32NumBytes - Value for 32 num bytes.
//!
//! @return Status code: `AM_DEVICES_MB85RC256V_STATUS_SUCCESS` on success;
//!         otherwise `AM_DEVICES_MB85RC256V_STATUS_*`.
//
//*****************************************************************************
static uint32_t
am_device_command_read(void *pHandle, uint8_t ui8DevAddr, uint32_t ui32InstrLen, uint64_t ui64Instr,
                       bool bCont, uint32_t *pData, uint32_t ui32NumBytes)
{
    am_hal_iom_transfer_t  Transaction;

    //
    // Create the transaction.
    //
    Transaction.ui32InstrLen    = ui32InstrLen;
#if (defined(AM_PART_APOLLO4B) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || \
     defined(AM_PART_APOLLO5_API) || defined(AM_PART_ATOMIQ110))
    Transaction.ui64Instr       = ui64Instr;
#else
    Transaction.ui32Instr       = (uint32_t)ui64Instr;
#endif
    Transaction.eDirection      = AM_HAL_IOM_RX;
    Transaction.ui32NumBytes    = ui32NumBytes;
    Transaction.pui32RxBuffer   = pData;
    Transaction.uPeerInfo.ui32I2CDevAddr = ui8DevAddr;
    Transaction.bContinue       = bCont;
    Transaction.ui8RepeatCount  = 0;
    Transaction.ui32PauseCondition = 0;
    Transaction.ui32StatusSetClr = 0;

    //
    // Execute the transction over IOM.
    //
    if (am_hal_iom_blocking_transfer(pHandle, &Transaction))
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }
    return AM_DEVICES_MB85RC256V_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Initialize the mb85rc256v driver.
//
//*****************************************************************************
uint32_t
am_devices_mb85rc256v_init(uint32_t ui32Module, am_devices_mb85rc256v_config_t *pDevConfig, void **ppHandle, void **ppIomHandle)
{
    void *pIomHandle;
    am_hal_iom_config_t     stIOMMB85RC256VSettings;
    uint32_t      ui32Index = 0;

    // Allocate a vacant device handle
    for ( ui32Index = 0; ui32Index < AM_DEVICES_MB85RC256V_MAX_DEVICE_NUM; ui32Index++ )
    {
        if ( gAmMb85rc256v[ui32Index].bOccupied == false )
        {
            break;
        }
    }
    if ( ui32Index == AM_DEVICES_MB85RC256V_MAX_DEVICE_NUM )
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }

    if ( (ui32Module > AM_REG_IOM_NUM_MODULES) || (pDevConfig == NULL) )
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }

    //
    // Enable fault detection.
    //
#if !defined(AM_PART_APOLLO5_API) && !defined(AM_PART_ATOMIQ110)
#if defined(AM_PART_APOLLO4_API)
    am_hal_fault_capture_enable();
#elif AM_PART_APOLLO3_API
    am_hal_mcuctrl_control(AM_HAL_MCUCTRL_CONTROL_FAULT_CAPTURE_ENABLE, NULL);
#else
    am_hal_mcuctrl_fault_capture_enable();
#endif
#endif

    stIOMMB85RC256VSettings = g_sIomMb85rc256vCfg;
    stIOMMB85RC256VSettings.ui32NBTxnBufLength = pDevConfig->ui32NBTxnBufLength;
    stIOMMB85RC256VSettings.pNBTxnBuf = pDevConfig->pNBTxnBuf;
    stIOMMB85RC256VSettings.ui32ClockFreq = pDevConfig->ui32ClockFreq;

    //
    // Initialize the IOM instance.
    // Enable power to the IOM instance.
    // Configure the IOM for Serial operation during initialization.
    // Enable the IOM.
    //
    if (am_hal_iom_initialize(ui32Module, &pIomHandle) ||
        am_hal_iom_power_ctrl(pIomHandle, AM_HAL_SYSCTRL_WAKE, false) ||
        am_hal_iom_configure(pIomHandle, &stIOMMB85RC256VSettings) ||
        am_hal_iom_enable(pIomHandle))
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }
    else
    {
        //
        // Configure the IOM pins.
        //
        am_bsp_iom_pins_enable(ui32Module, AM_HAL_IOM_I2C_MODE);

        gAmMb85rc256v[ui32Index].bOccupied = true;
        gAmMb85rc256v[ui32Index].ui32Module = ui32Module;
        *ppIomHandle = gAmMb85rc256v[ui32Index].pIomHandle = pIomHandle;
        *ppHandle = (void *)&gAmMb85rc256v[ui32Index];
        //
        // Return the status.
        //
        return AM_DEVICES_MB85RC256V_STATUS_SUCCESS;
    }
}

//*****************************************************************************
//
// De-Initialize the mb85rc256v driver.
//
//*****************************************************************************
uint32_t
am_devices_mb85rc256v_term(void *pHandle)
{
    am_devices_iom_mb85rc256v_t *pIom = (am_devices_iom_mb85rc256v_t *)pHandle;

    if ( pIom->ui32Module > AM_REG_IOM_NUM_MODULES )
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }

// #### INTERNAL BEGIN ####
    // TODO - no bsp function to disable pins
// #### INTERNAL END ####
    //
    // Disable the IOM.
    //
    am_hal_iom_disable(pIom->pIomHandle);

    //
    // Disable power to and uninitialize the IOM instance.
    //
    am_hal_iom_power_ctrl(pIom->pIomHandle, AM_HAL_SYSCTRL_DEEPSLEEP, false);

    am_hal_iom_uninitialize(pIom->pIomHandle);

    // Free this device handle
    pIom->bOccupied = false;

    //
    // Configure the IOM pins.
    //
    am_bsp_iom_pins_disable(pIom->ui32Module, AM_HAL_IOM_I2C_MODE);

    //
    // Return the status.
    //
    return AM_DEVICES_MB85RC256V_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Reads the ID of the external flash and returns the value.
//
//*****************************************************************************
uint32_t
am_devices_mb85rc256v_read_id(void *pHandle, uint32_t *pDeviceID)
{
    am_devices_iom_mb85rc256v_t *pIom = (am_devices_iom_mb85rc256v_t *)pHandle;

    //
    // Send the command sequence to read the Device ID.
    //
    if (am_device_command_read(pIom->pIomHandle, AM_DEVICES_MB85RC256V_RSVD_PERIPHERAL_ID, 1,
                           (AM_DEVICES_MB85RC256V_PERIPHERAL_ID << 1),
                           false, pDeviceID, 3))
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }

    //
    // Return the status.
    //
    return AM_DEVICES_MB85RC256V_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Programs the given range of flash addresses.
//
//*****************************************************************************
uint32_t
am_devices_mb85rc256v_blocking_write(void *pHandle, uint8_t *pui8TxBuffer,
                                     uint32_t ui32WriteAddress,
                                     uint32_t ui32NumBytes)
{
    am_devices_iom_mb85rc256v_t *pIom = (am_devices_iom_mb85rc256v_t *)pHandle;

    //
    // Write the data to the device.
    //
    if (am_device_command_write(pIom->pIomHandle, AM_DEVICES_MB85RC256V_PERIPHERAL_ID, 2,
                            (ui32WriteAddress & 0x0000FFFF),
                            false, (uint32_t *)pui8TxBuffer, ui32NumBytes))
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }

    //
    // Return the status.
    //
    return AM_DEVICES_MB85RC256V_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Programs the given range of flash addresses.
//
//*****************************************************************************
uint32_t
am_devices_mb85rc256v_nonblocking_write(void *pHandle, uint8_t *pui8TxBuffer,
                                        uint32_t ui32WriteAddress,
                                        uint32_t ui32NumBytes,
                                        am_hal_iom_callback_t pfnCallback,
                                        void *pCallbackCtxt)
{
    am_hal_iom_transfer_t         Transaction;
    am_devices_iom_mb85rc256v_t   *pIom = (am_devices_iom_mb85rc256v_t *)pHandle;

    Transaction.ui8RepeatCount  = 0;
    Transaction.ui32PauseCondition = 0;
    Transaction.ui32StatusSetClr = 0;
    Transaction.ui8Priority     = 1;        // High priority for now.
    Transaction.uPeerInfo.ui32I2CDevAddr = AM_DEVICES_MB85RC256V_PERIPHERAL_ID;
    Transaction.bContinue       = false;

    //
    // Set up the IOM transaction.
    //
    Transaction.eDirection      = AM_HAL_IOM_TX;
    Transaction.ui32InstrLen    = 2;
#if (defined(AM_PART_APOLLO4B) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || \
     defined(AM_PART_APOLLO5_API) || defined(AM_PART_ATOMIQ110))
    Transaction.ui64Instr       = ui32WriteAddress & 0x0000FFFF;
#else
    Transaction.ui32Instr       = ui32WriteAddress & 0x0000FFFF;
#endif
    Transaction.ui32NumBytes    = ui32NumBytes;
    Transaction.pui32TxBuffer   = (uint32_t *)pui8TxBuffer;

    //
    // Add this transaction to the command queue (no callback).
    //
    if (am_hal_iom_nonblocking_transfer(pIom->pIomHandle, &Transaction, pfnCallback, pCallbackCtxt))
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }

    //
    // Return the status.
    //
    return AM_DEVICES_MB85RC256V_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Reads the contents of the FRAM into a buffer.
//
//*****************************************************************************
uint32_t
am_devices_mb85rc256v_blocking_read(void *pHandle, uint8_t *pui8RxBuffer,
                                    uint32_t ui32ReadAddress,
                                    uint32_t ui32NumBytes)
{
    am_devices_iom_mb85rc256v_t   *pIom = (am_devices_iom_mb85rc256v_t *)pHandle;

    if (am_device_command_read(pIom->pIomHandle, AM_DEVICES_MB85RC256V_PERIPHERAL_ID, 2,
                           (ui32ReadAddress & 0x0000FFFF),
                           false, (uint32_t *)pui8RxBuffer, ui32NumBytes))
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }

    //
    // Return the status.
    //
    return AM_DEVICES_MB85RC256V_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Reads the contents of the FRAM into a buffer.
//
//*****************************************************************************
uint32_t
am_devices_mb85rc256v_nonblocking_read(void *pHandle, uint8_t *pui8RxBuffer,
                                       uint32_t ui32ReadAddress,
                                       uint32_t ui32NumBytes,
                                       am_hal_iom_callback_t pfnCallback,
                                       void * pCallbackCtxt)
{
    am_hal_iom_transfer_t Transaction;
    am_devices_iom_mb85rc256v_t   *pIom = (am_devices_iom_mb85rc256v_t *)pHandle;

    //
    // Set up the IOM transaction.
    //
    Transaction.ui8Priority     = 1;        // High priority for now.
    Transaction.eDirection      = AM_HAL_IOM_RX;
    Transaction.ui32InstrLen    = 2;
#if (defined(AM_PART_APOLLO4B) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || \
     defined(AM_PART_APOLLO5_API) || defined(AM_PART_ATOMIQ110))
    Transaction.ui64Instr       = (ui32ReadAddress & 0x0000FFFF);
#else
    Transaction.ui32Instr       = (ui32ReadAddress & 0x0000FFFF);
#endif
    Transaction.ui32NumBytes    = ui32NumBytes;
    Transaction.pui32RxBuffer   = (uint32_t *)pui8RxBuffer;
    Transaction.uPeerInfo.ui32I2CDevAddr = AM_DEVICES_MB85RC256V_PERIPHERAL_ID;
    Transaction.ui8RepeatCount  = 0;
    Transaction.ui32PauseCondition = 0;
    Transaction.ui32StatusSetClr = 0;
    Transaction.bContinue       = false;

    //
    // Start the transaction.
    //
    if (am_hal_iom_nonblocking_transfer(pIom->pIomHandle, &Transaction, pfnCallback, pCallbackCtxt))
    {
        return AM_DEVICES_MB85RC256V_STATUS_ERROR;
    }

    //
    // Return the status.
    //
    return AM_DEVICES_MB85RC256V_STATUS_SUCCESS;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

