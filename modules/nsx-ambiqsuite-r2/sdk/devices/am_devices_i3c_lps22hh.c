//*****************************************************************************
//
//! @file am_devices_i3c_lps22hh.c
//!
//! @brief ST Microelectronics LPS22HH Pressure and Temperature Sensor Driver with I3C Interface.
//!
//! @addtogroup devices_i3c_lps22hh LPS22HH I3C Pressure/Temperature Sensor Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a comprehensive hardware abstraction layer
//!          for the ST Microelectronics LPS22HH digital barometric pressure sensor.
//!          The driver enables high-precision atmospheric pressure and temperature
//!          measurement through the I3C interface. It supports advanced features
//!          including configurable output data rates, pressure and temperature
//!          data acquisition, I3C Common Command Codes (CCC) for dynamic addressing,
//!          and efficient data management for environmental sensing applications.
//!
//! @section devices_i3c_lps22hh_features Key Features
//!
//! 1. @b High-Precision @b Pressure @b Sensor: 24-bit pressure data with excellent accuracy.
//! 2. @b Temperature @b Sensor: Integrated temperature measurement for compensation.
//! 3. @b I3C @b Interface: High-speed digital communication with CCC support.
//! 4. @b Dynamic @b Addressing: I3C SETDASA and RSTDAA command support.
//! 5. @b Configurable @b ODR: Multiple output data rates from 1Hz to 200Hz.
//! 6. @b Low @b Power @b Modes: Optimized power consumption for battery applications.
//! 7. @b FIFO @b Support: Internal FIFO buffer for data buffering and batch processing.
//! 8. @b DMA @b Support: Efficient data transfer with timeout and callback mechanisms.
//!
//! @section devices_i3c_lps22hh_functionality Functionality
//!
//! - Initialize and configure LPS22HH pressure sensor via I3C interface
//! - Read high-precision pressure data in Pascal units
//! - Read temperature data for environmental monitoring and sensor compensation
//! - Configure sensor output data rates and operating modes
//! - Handle I3C Common Command Codes for device management
//! - Support dynamic I3C address assignment and reset operations
//! - Provide device identification and status monitoring
//! - Manage I3C read/write length limitations with SETMWL/SETMRL commands
//! - Support register-level read/write operations for advanced configuration
//!
//! @section devices_i3c_lps22hh_usage Usage
//!
//! 1. Initialize I3C host interface and configure I3C module
//! 2. Initialize LPS22HH device with am_devices_i3c_lps22hh_init()
//! 3. Configure sensor parameters (ODR, operating modes) through device_init
//! 4. Read device ID to verify communication and device presence
//! 5. Read pressure data with am_devices_i3c_lps22hh_get_pressure()
//! 6. Read temperature data with am_devices_i3c_lps22hh_get_temperature()
//! 7. Use register access functions for advanced configuration
//! 8. Monitor device status and handle data availability flags
//! 9. Terminate device with am_devices_i3c_lps22hh_term() when finished
//!
//! @section devices_i3c_lps22hh_configuration Configuration
//!
//! - Set up I3C interface parameters and timing
//! - Configure pressure sensor output data rate and operating mode
//! - Set up I3C dynamic addressing and device provisioning
//! - Configure DMA parameters for efficient data transfer
//! - Set up FIFO configurations for data buffering
//! - Configure interrupt settings for data ready notifications
//! - Optimize power consumption through ODR and low-power modes
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
#include <string.h>
#include <stdlib.h>
#include "am_mcu_apollo.h"
#include "am_devices_i3c_lps22hh.h"
#include "am_bsp.h"
#include "am_util.h"

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************
typedef struct
{
    uint32_t                ui32Module;
    void                    *pI3cHandle;
    bool                    bOccupied;
    uint8_t                 ui8StaticAddr;
    am_devices_i3c_lps22hh_config_t sI3cLps22hhCfg;
} am_devices_i3c_lps22hh_t;

am_devices_i3c_lps22hh_t gAmI3cLps22hh[AM_DEVICES_LPS22HH_MAX_DEVICE_NUM];

//*****************************************************************************
//
//  LPS22HH device driver default callback to wait DMA complete
//
//*****************************************************************************
static uint32_t
am_devices_i3c_lps22hh_timeout_callback(bool *pDMAComplete)
{
    uint32_t ui32Timeout = AM_DEVICES_LPS22HH_DMA_TIMEOUT_CNT;
    //
    // Wait until DMA transfer done
    //
    while (!(*(volatile bool *)pDMAComplete))
    {
        if (ui32Timeout == 0)
        {
            break;
        }
        am_util_delay_us(AM_DEVICES_LPS22HH_DMA_DELAY_US);
        am_util_debug_printf("waiting non-blocking complete\n");
        ui32Timeout--;
    }

    if ( ui32Timeout == 0 )
    {
        am_util_stdio_printf("Non-blocking timeout\n");
        return AM_DEVICES_LPS22HH_STATUS_TIMEOUT;
    }

    return AM_DEVICES_LPS22HH_STATUS_SUCCESS;
}
//*****************************************************************************
//
//  LPS22HH device data transfer based on the given sub address of sensor,
//  bytes number and transfer direction.
//
//*****************************************************************************
static uint32_t
am_devices_i3c_lps22hh_xfer(void *pHandle,
                            uint8_t ui8SubAddr,
                            uint32_t *pData,
                            uint32_t ui32NumBytes,
                            bool bRead)
{
    am_hal_i3c_transfer_t Transaction;
    am_devices_i3c_lps22hh_t *pDevHandle = (am_devices_i3c_lps22hh_t *)pHandle;
    am_devices_i3c_lps22hh_config_t *pDeviceCfg = &pDevHandle->sI3cLps22hhCfg;

    if ( !pHandle )
    {
        return AM_DEVICES_LPS22HH_STATUS_INVALID_HANDLE;
    }

    Transaction.Device.ui8DynamicAddr = pDeviceCfg->ui8DynamicAddr;
    Transaction.Device.eDeviceType = AM_HAL_I3C_DEVICE_I3C;

    //
    // Create the transaction
    //
    Transaction.ui32NumBytes  = ui32NumBytes;
    Transaction.eSpeedMode    = pDeviceCfg->eSpeedMode;

    if (bRead)
    {
        Transaction.bComboXfer = true;
        Transaction.eDirection = AM_HAL_I3C_DIR_READ;
        Transaction.pui32RxBuffer = pData;
    }
    else
    {
        //
        // Save target reg address to write buffer
        //
        uint8_t *pui8Buffer = (uint8_t *)pData;
        pui8Buffer[0] = ui8SubAddr;
        Transaction.bComboXfer = false;
        Transaction.eDirection = AM_HAL_I3C_DIR_WRITE;
        Transaction.ui32NumBytes = ui32NumBytes + 1;
        Transaction.pui32TxBuffer = pData;
    }

    Transaction.bCombo16bitOfs = false;
    Transaction.ui16ComboOfs = ui8SubAddr;

    //
    // Execute the transction over I3C.
    //
    if (pDeviceCfg->eTransferMode == AM_HAL_I3C_XFER_DMA)
    {
        Transaction.pui32CmdRingBuf     = pDeviceCfg->sI3cDmaCfg.pCmdRingBuf;
        Transaction.ui32CmdRingBufLen   = pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen;
        Transaction.pui32RespRingBuf    = pDeviceCfg->sI3cDmaCfg.pRespRingBuf;
        Transaction.ui32RespRingBufLen  = pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen;

        *(volatile bool *)pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;

        if ( am_hal_i3c_nonblocking_transfer(pDevHandle->pI3cHandle, &Transaction)  != AM_HAL_STATUS_SUCCESS )
        {
            return AM_DEVICES_LPS22HH_STATUS_ERROR;
        }

        //
        // Wait until DMA transfer done
        //
        pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback(pDeviceCfg->sI3cDmaCfg.pDMAComplete);
    }
    else
    {
        if ( am_hal_i3c_blocking_transfer(pDevHandle->pI3cHandle, &Transaction) != AM_HAL_STATUS_SUCCESS)
        {
            return AM_DEVICES_LPS22HH_STATUS_ERROR;
        }

        am_hal_delay_us(AM_DEVICES_LPS22HH_TRANSFER_DELAY_TIME);

    }

    return AM_DEVICES_LPS22HH_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Reads the device id of the sensor.
//
//*****************************************************************************
static uint32_t
am_devices_i3c_lps22hh_read_id(void *pHandle, uint32_t *pDeviceID)
{
    if ( !pHandle )
    {
        return AM_DEVICES_LPS22HH_STATUS_INVALID_HANDLE;
    }

    if ( !pDeviceID )
    {
        return AM_DEVICES_LPS22HH_STATUS_INVALID_ARG;
    }

    return am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_WHO_AMI_I, pDeviceID, 1, true);
}

//*****************************************************************************
//
//! @brief Send I3C Common Command Codes
//!
//! @param pHandle       - Pointer to device handle
//! @param pui32Data     - Buffer to config sensor or save current configuration
//! @param ui8CccId      - Common command code index
//!
//! This function sends I3C CCCs to sensor to configure or get sensor information
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_i3c_lps22hh_send_cccs(void *pHandle, uint32_t *pui32Data, uint8_t ui8CccId)
{
    uint32_t ui32Status = AM_DEVICES_LPS22HH_STATUS_SUCCESS;

    if ( !pHandle )
    {
        return AM_DEVICES_LPS22HH_STATUS_INVALID_HANDLE;
    }

    am_devices_i3c_lps22hh_t *pDevHandle = (am_devices_i3c_lps22hh_t *)pHandle;
    am_devices_i3c_lps22hh_config_t *pDeviceCfg = &pDevHandle->sI3cLps22hhCfg;
    am_hal_i3c_transfer_t Transaction;

    Transaction.Device.eDeviceType = AM_HAL_I3C_DEVICE_I3C;
    Transaction.eSpeedMode     = pDeviceCfg->eSpeedMode;
    Transaction.ui8CCCID       = ui8CccId;
    Transaction.bComboXfer     = false;
    Transaction.bCombo16bitOfs = false;
    Transaction.ui16ComboOfs   = 0;

    Transaction.Device.ui8DynamicAddr = pDeviceCfg->ui8DynamicAddr;
    Transaction.Device.ui8StaticAddr  = pDevHandle->ui8StaticAddr;

    if ( pDeviceCfg->eTransferMode == AM_HAL_I3C_XFER_DMA )
    {
        *(volatile bool *)pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;
        Transaction.pui32CmdRingBuf     = pDeviceCfg->sI3cDmaCfg.pCmdRingBuf;
        Transaction.ui32CmdRingBufLen   = pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen;
        Transaction.pui32RespRingBuf    = pDeviceCfg->sI3cDmaCfg.pRespRingBuf;
        Transaction.ui32RespRingBufLen  = pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen;
    }

    switch(ui8CccId)
    {
        case AM_HAL_I3C_CCC_ENEC(false):
        case AM_HAL_I3C_CCC_ENEC(true):
        case AM_HAL_I3C_CCC_DISEC(false):
        case AM_HAL_I3C_CCC_DISEC(true):
        case AM_HAL_I3C_CCC_SETNEWDA:
        case AM_HAL_I3C_CCC_SETDASA:
        case AM_HAL_I3C_CCC_ENTDAA:
        case AM_HAL_I3C_CCC_RSTDAA(true):
        case AM_HAL_I3C_CCC_RSTDAA(false):
            Transaction.ui32NumBytes = 1;
            Transaction.eDirection = AM_HAL_I3C_DIR_WRITE;
            break;
        case AM_HAL_I3C_CCC_SETMWL(false):
        case AM_HAL_I3C_CCC_SETMWL(true):
            Transaction.ui32NumBytes = 2;
            Transaction.eDirection = AM_HAL_I3C_DIR_WRITE;
            break;
        case AM_HAL_I3C_CCC_SETMRL(false):
        case AM_HAL_I3C_CCC_SETMRL(true):
            Transaction.ui32NumBytes = 3;
            Transaction.eDirection = AM_HAL_I3C_DIR_WRITE;
            break;
        case AM_HAL_I3C_CCC_SETXTIME(false):
        case AM_HAL_I3C_CCC_SETXTIME(true):
            Transaction.ui32NumBytes = 4;
            Transaction.eDirection = AM_HAL_I3C_DIR_WRITE;
            break;
        case AM_HAL_I3C_CCC_GETBCR:
        case AM_HAL_I3C_CCC_GETDCR:
            Transaction.ui32NumBytes = 1;
            Transaction.eDirection = AM_HAL_I3C_DIR_READ;
            break;
        case AM_HAL_I3C_CCC_GETMWL:
        case AM_HAL_I3C_CCC_GETSTATUS:
        case AM_HAL_I3C_CCC_GETMXDS:
            Transaction.ui32NumBytes = 2;
            Transaction.eDirection = AM_HAL_I3C_DIR_READ;
            break;
        case AM_HAL_I3C_CCC_GETMRL:
            Transaction.ui32NumBytes = 3;
            Transaction.eDirection = AM_HAL_I3C_DIR_READ;
            break;
        case AM_HAL_I3C_CCC_GETPID:
            Transaction.ui32NumBytes = 6;
            Transaction.eDirection = AM_HAL_I3C_DIR_READ;
            break;
        case AM_HAL_I3C_CCC_GETXTIME:
            Transaction.ui32NumBytes = 4;
            Transaction.eDirection = AM_HAL_I3C_DIR_READ;
            break;
        default :
            return AM_DEVICES_LPS22HH_STATUS_INVALID_ARG;
    }

    if ( Transaction.eDirection == AM_HAL_I3C_DIR_WRITE )
    {
        Transaction.pui32TxBuffer = pui32Data;
    }
    else
    {
        Transaction.pui32RxBuffer = pui32Data;
    }

    if ( Transaction.ui8CCCID == AM_HAL_I3C_CCC_ENTDAA || Transaction.ui8CCCID == AM_HAL_I3C_CCC_SETDASA )
    {
        ui32Status = am_hal_i3c_do_daa(pDevHandle->pI3cHandle, &Transaction);
    }
    else
    {
        ui32Status = am_hal_i3c_send_ccc_cmd(pDevHandle->pI3cHandle, &Transaction);
    }

    am_hal_delay_us(AM_DEVICES_LPS22HH_TRANSFER_DELAY_TIME);

    if (pDeviceCfg->eTransferMode == AM_HAL_I3C_XFER_DMA)
    {
        //
        // Wait until DMA transfer done
        //
        pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback(pDeviceCfg->sI3cDmaCfg.pDMAComplete);
    }

     return ui32Status;
}

//*****************************************************************************
//
// LPS22HH device configuration to initiliaze sensor
//
//*****************************************************************************
static uint32_t
am_devices_i3c_lps22hh_device_init(void *pHandle)
{
    uint32_t ui32Reg = 0;
    uint32_t ui32Status = AM_DEVICES_LPS22HH_STATUS_SUCCESS;

    //
    // Set lps22hh max write/read length
    //
    uint32_t ui32MaxWrLen = AM_DEVICES_LPS22HH_MAX_WRITE_LENGTH;
    uint32_t ui32MaxRdLen = AM_DEVICES_LPS22HH_MAX_READ_LENGTH;

    ui32Status = am_devices_i3c_lps22hh_send_cccs(pHandle, &ui32MaxWrLen, AM_HAL_I3C_CCC_SETMWL(false));
    if (AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Failed to set lps22hh max write length!\n");
        return ui32Status;
    }

    ui32Status = am_devices_i3c_lps22hh_send_cccs(pHandle, &ui32MaxRdLen, AM_HAL_I3C_CCC_SETMRL(false));
    if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
    {
        am_util_stdio_printf("Failed to set lps22hh max read length!\n");
        return ui32Status;
    }

    //
    // Lps22hh software reset, restore the default values in user registers.[set]
    //
    if ( !am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_CTRL_REG2, &ui32Reg, 1, true) )
    {
        ui32Reg |= AM_DEVICES_LPS22HH_RESET_MSK;
        ui32Reg = ui32Reg << 8;
        ui32Status = am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_CTRL_REG2, &ui32Reg, 1, false);
        if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
        {
            am_util_stdio_printf("Failed read lps22hh reg:0x%x!\n", AM_DEVICES_LPS22HH_CTRL_REG2);
            return ui32Status;
        }
    }

    //
    // Get Lps22hh software reset status
    //
    uint32_t ui32Timeout = AM_DEVICES_LPS22HH_DMA_TIMEOUT_CNT;
    do
    {
        ui32Status = am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_CTRL_REG2, &ui32Reg, 1, true);
        if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
        {
            am_util_stdio_printf("Failed read lps22hh reg:0x%x!\n", AM_DEVICES_LPS22HH_CTRL_REG2);
            return ui32Status;
        }
        ui32Timeout--;

        if ( ui32Timeout == 0)
        {
            am_util_stdio_printf("Lps22hh software reset timeout\n");
            return AM_DEVICES_LPS22HH_STATUS_TIMEOUT;
        }
    }
    while (ui32Reg & AM_DEVICES_LPS22HH_RESET_MSK);

    //
    // Enable Block Data Update
    //
    uint32_t ui32CtrlReg1 = 0;
    if (!am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_CTRL_REG1, &ui32CtrlReg1, 1, true))
    {
        ui32CtrlReg1 |= AM_DEVICES_LPS22HH_BLOCK_DATA_MSK;
        ui32CtrlReg1 = ui32CtrlReg1 << 8;
        ui32Status = am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_CTRL_REG1, &ui32CtrlReg1, 1, false);
        if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
        {
            am_util_stdio_printf("Failed write lps22hh reg:0x%x!\n", AM_DEVICES_LPS22HH_CTRL_REG1);
            return ui32Status;
        }
    }

    //
    // Set Output Data Rate
    //
    uint32_t ui32CtrlReg2 = 0;
    ui32Status = am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_CTRL_REG1, &ui32CtrlReg1, 1, true);
    if (ui32Status == AM_DEVICES_LPS22HH_STATUS_SUCCESS)
    {
        ui32Status = am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_CTRL_REG2, &ui32CtrlReg2, 1, true);
        if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
        {
            am_util_stdio_printf("Failed read lps22hh reg:0x%x!\n", AM_DEVICES_LPS22HH_CTRL_REG2);
            return ui32Status;
        }
    }
    else
    {
        am_util_stdio_printf("Failed read lps22hh reg:0x%x!\n", AM_DEVICES_LPS22HH_CTRL_REG1);
        return ui32Status;
    }

    ui32CtrlReg1 |= (AM_DEVICES_LPS22HH_10_Hz_LOW_NOISE & 0x7) << 4;
    ui32CtrlReg1 = ui32CtrlReg1 << 8;
    ui32Status = am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_CTRL_REG1, &ui32CtrlReg1, 1, false);
    if (ui32Status == AM_DEVICES_LPS22HH_STATUS_SUCCESS)
    {
        ui32CtrlReg2 |= (AM_DEVICES_LPS22HH_10_Hz_LOW_NOISE & 0x10) >> 4;
        ui32CtrlReg2 |= (AM_DEVICES_LPS22HH_10_Hz_LOW_NOISE & 0x08) >> 3;
        ui32CtrlReg2 = ui32CtrlReg2 << 8;
        ui32Status = am_devices_i3c_lps22hh_xfer(pHandle, AM_DEVICES_LPS22HH_CTRL_REG2, &ui32CtrlReg2, 1, false);
        if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
        {
            am_util_stdio_printf("Failed write lps22hh reg:0x%x!\n", AM_DEVICES_LPS22HH_CTRL_REG2);
            return ui32Status;
        }
    }
    else
    {
        am_util_stdio_printf("Failed write lps22hh reg:0x%x!\n", AM_DEVICES_LPS22HH_CTRL_REG1);
        return ui32Status;
    }

    return AM_DEVICES_LPS22HH_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Initialize the lps22hh driver.
//!
//! @param ui32Module  - I3C instance
//! @param pI3cHandle  - I3C controller handle
//! @param ppDevHandle - Device handle which needs to be return
//! @param pDevConfig  - I3C device structure describing the target sensor
//!
//! @note This function should be called before any other am_devices_i3c_lps22hh
//! functions. It is used to tell the other functions how to communicate
//! with the other I3C sensors.
//!
//! @return Status.
//
//*****************************************************************************
uint32_t
am_devices_i3c_lps22hh_init(uint32_t ui32Module,
                            void *pI3cHandle,
                            void **ppDevHandle,
                            am_devices_i3c_lps22hh_config_t *pDevConfig)
{
    uint32_t ui32Index  = 0;
    uint32_t ui32DeviceId = 0;
    uint32_t ui32Status = AM_DEVICES_LPS22HH_STATUS_SUCCESS;

    am_devices_i3c_lps22hh_config_t *pDeviceCfg;
    am_devices_i3c_lps22hh_t *pDevHandle;

    //
    // Allocate a vacant device handle
    //
    for ( ui32Index = 0; ui32Index < AM_DEVICES_LPS22HH_MAX_DEVICE_NUM; ui32Index++ )
    {
        if ( gAmI3cLps22hh[ui32Index].bOccupied == false )
        {
            pDevHandle = &gAmI3cLps22hh[ui32Index];
            pDeviceCfg = &gAmI3cLps22hh[ui32Index].sI3cLps22hhCfg;
            break;
        }
    }

    if ( ui32Index == AM_DEVICES_LPS22HH_MAX_DEVICE_NUM )
    {
        return AM_DEVICES_LPS22HH_STATUS_OUT_OF_RANGE;
    }

    if ( (ui32Module > AM_REG_I3C_NUM_MODULES) || (pDevConfig == NULL) )
    {
        return AM_DEVICES_LPS22HH_STATUS_INVALID_HANDLE;
    }

#if defined(apollo510L_eb)
    pDevHandle->ui8StaticAddr      = AM_DEVICES_LPS22HH_STATIC_ADDRESS_1;
#elif defined(apollo510L_val)
    pDevHandle->ui8StaticAddr      = AM_DEVICES_LPS22HH_STATIC_ADDRESS_2;
#else
    pDevHandle->ui8StaticAddr      = AM_DEVICES_LPS22HH_STATIC_ADDRESS_1;
#endif

    pDeviceCfg->eTransferMode  = pDevConfig->eTransferMode;
    pDeviceCfg->eSpeedMode     = pDevConfig->eSpeedMode;
    pDeviceCfg->ui8DynamicAddr = pDevConfig->ui8DynamicAddr;

    gAmI3cLps22hh[ui32Index].bOccupied = true;
    gAmI3cLps22hh[ui32Index].ui32Module = ui32Module;
    gAmI3cLps22hh[ui32Index].pI3cHandle = pI3cHandle;
    *ppDevHandle = pDevHandle;

    if ( pDevConfig->eTransferMode == AM_HAL_I3C_XFER_DMA )
    {
        if ( pDevConfig->sI3cDmaCfg.pDMAComplete == NULL )
        {
            return AM_DEVICES_LPS22HH_STATUS_INVALID_ARG;
        }

        if ( pDevConfig->sI3cDmaCfg.pfnI3cTimeoutCallback == NULL )
        {
            pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback = am_devices_i3c_lps22hh_timeout_callback;
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
    // Send RSTDAA ccc to sensor
    //
    uint32_t ui32CmdData = 0;
    ui32Status = am_devices_i3c_lps22hh_send_cccs(&gAmI3cLps22hh[ui32Index], &ui32CmdData, AM_HAL_I3C_CCC_RSTDAA(true));
    if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
    {
        am_util_stdio_printf("Failed to reset lps22hh dasa!\n");
        return ui32Status;
    }

    //
    // Send SETDASA ccc to set the dynamic address of sensor
    //
    ui32Status = am_devices_i3c_lps22hh_send_cccs(&gAmI3cLps22hh[ui32Index], &ui32CmdData, AM_HAL_I3C_CCC_SETDASA);
    if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
    {
        am_util_stdio_printf("Failed to set lps22hh dasa!\n");
        return ui32Status;
    }

    //
    // Read device ID
    //
    ui32Status = am_devices_i3c_lps22hh_read_id(&gAmI3cLps22hh[ui32Index], &ui32DeviceId);
    if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
    {
        am_util_stdio_printf("Failed to read device id!\n");
        return ui32Status;
    }

    if ( (ui32DeviceId & 0xFF) == AM_DEVICES_LPS22HH_DEVICE_ID)
    {
        am_util_stdio_printf("Get lps22hh device id:0x%x!\n", ui32DeviceId);
    }
    else
    {
        am_util_stdio_printf("Get lps22hh device id failed!\n");
        return AM_DEVICES_LPS22HH_STATUS_ERROR;
    }

    ui32Status = am_devices_i3c_lps22hh_device_init(&gAmI3cLps22hh[ui32Index]);
    if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
    {
        am_util_stdio_printf("Failed to config lps22hh!\n");
        return ui32Status;
    }

    return AM_DEVICES_LPS22HH_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief De-Initialize the lps22hh driver.
//!
//! @param pHandle     - Pointer to device handle
//!
//! This function reverses the initialization
//!
//! @return Status.
//
//*****************************************************************************
uint32_t
am_devices_i3c_lps22hh_term(void *pHandle)
{
    am_devices_i3c_lps22hh_t *pDevHandle = (am_devices_i3c_lps22hh_t *)pHandle;
    am_devices_i3c_lps22hh_config_t *pDeviceCfg = &pDevHandle->sI3cLps22hhCfg;
    uint32_t ui32Status = AM_DEVICES_LPS22HH_STATUS_SUCCESS;

    if ( pDevHandle->ui32Module > AM_REG_I3C_NUM_MODULES )
    {
        return AM_DEVICES_LPS22HH_STATUS_OUT_OF_RANGE;
    }

    //
    // Send RSTDAA ccc to ensure all device'Dynamic Address cleared
    //
    uint32_t ui32CmdData = 0;
    ui32Status = am_devices_i3c_lps22hh_send_cccs(pDevHandle, &ui32CmdData, AM_HAL_I3C_CCC_RSTDAA(true));
    if ( AM_DEVICES_LPS22HH_STATUS_SUCCESS != ui32Status )
    {
        am_util_stdio_printf("Failed to reset lps22hh dasa!\n");
        return ui32Status;
    }

    //
    // Free this device handle
    //
    pDevHandle->bOccupied = false;
    pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback = NULL;
    pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;

    //
    // Return the status.
    //
    return AM_DEVICES_LPS22HH_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Write generic device register of lps22hh sensor.
//!
//! @param pHandle        - Pointer to device handle
//! @param ui8SubAddress  - Address of desired data in sensor
//! @param pData          - Buffer to write the sensor data from
//! @param ui32NumBytes   - Number of bytes to write to sensor
//!
//! This function write data to the given register of sensor, and returns
//! the result as an 32-bit unsigned integer value.
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_i3c_lps22hh_write_reg(void *pHandle, uint8_t ui8SubAddress,
                                 uint32_t *pData, uint32_t ui32NumBytes)
{
    if ( !pHandle )
    {
        return AM_DEVICES_LPS22HH_STATUS_INVALID_HANDLE;
    }

    if ( !pData )
    {
        return AM_DEVICES_LPS22HH_STATUS_INVALID_ARG;
    }

    return am_devices_i3c_lps22hh_xfer(pHandle, ui8SubAddress, pData, ui32NumBytes, false);
}

//*****************************************************************************
//
//! @brief Read generic device register from lps22hh sensor.
//!
//! @param pHandle        - Pointer to device handle
//! @param ui8SubAddress  - Address of desired data in sensor
//! @param pData          - Buffer to store the received data from sensor
//! @param ui32NumBytes   - Number of bytes to read from sensor
//!
//! This function read data from the given register of sensor, and returns
//! the result as an 32-bit unsigned integer value.
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_i3c_lps22hh_read_reg(void *pHandle, uint8_t ui8SubAddress,
                                uint32_t *pData, uint32_t ui32NumBytes)
{
    if ( !pHandle )
    {
        return AM_DEVICES_LPS22HH_STATUS_INVALID_HANDLE;
    }

    if ( !pData )
    {
        return AM_DEVICES_LPS22HH_STATUS_INVALID_ARG;
    }

    return am_devices_i3c_lps22hh_xfer(pHandle, ui8SubAddress, pData, ui32NumBytes, true);
}

//*****************************************************************************
//
//! @brief Get the pressure from lps22hh sensor.
//!
//! @param pHandle        - Pointer to device handle
//! @param pData          - The actual pressure data detected by sensor
//!
//! This function read data from the sensor pressure registers, and caculate
//! the pressure data based on registers data.
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_i3c_lps22hh_get_pressure(void *pHandle, float32_t *pData)
{
    uint32_t ui32RxBuffer = 0;

    //
    // Get pressure output high, mid and low registers' data of sensor
    //
    if (am_devices_i3c_lps22hh_read_reg(pHandle, AM_DEVICES_LPS22HH_PRESS_OUT_XL, &ui32RxBuffer, 3))
    {
        return AM_DEVICES_LPS22HH_STATUS_ERROR;
    }

    ui32RxBuffer = (ui32RxBuffer & 0xFFFFFF) * 256U;

    //
    // Caculate the pressure data according the formulas of sensor spec
    //
    *pData = ((float32_t) ui32RxBuffer / 1048576.0f);

    return AM_DEVICES_LPS22HH_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Get the temperature from lps22hh sensor.
//!
//! @param pHandle        - Pointer to device handle
//! @param pData          - The actual temperature data detected by sensor
//!
//! This function read data from the sensor temperature registers, and caculate
//! the temperature data based on registers data.
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_i3c_lps22hh_get_temperature(void *pHandle, float32_t *pData)
{
    uint32_t ui32RxBuffer = 0;

    //
    // Get temperature output high and low registers' data of sensor
    //
    if (am_devices_i3c_lps22hh_read_reg(pHandle, AM_DEVICES_LPS22HH_TEMP_OUT_L, &ui32RxBuffer, 2))
    {
        return AM_DEVICES_LPS22HH_STATUS_ERROR;
    }

    //
    // Caculate the temperature data according the formulas of sensor spec
    //
    *pData = ((float32_t) (ui32RxBuffer & 0xFFFF) / 100.0f);

    return AM_DEVICES_LPS22HH_STATUS_SUCCESS;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
