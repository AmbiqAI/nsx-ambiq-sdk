//*****************************************************************************
//
//! @file am_devices_i3c_lsm6dso.c
//!
//! @brief ST Microelectronics LSM6DSO IMU Sensor Driver with I3C Interface.
//!
//! @addtogroup devices_i3c_lsm6dso LSM6DSO I3C IMU Sensor Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a comprehensive hardware abstraction layer
//!          for the ST Microelectronics LSM6DSO inertial measurement unit (IMU).
//!          The driver enables high-performance 3-axis accelerometer and gyroscope
//!          data acquisition through the I3C interface. It supports advanced features
//!          including configurable output data rates, full-scale ranges, temperature
//!          sensing, and I3C Common Command Codes (CCC) for dynamic addressing
//!          and device management.
//!
//! @section devices_i3c_lsm6dso_features Key Features
//!
//! 1. @b 3-Axis @b Accelerometer: High-precision linear acceleration measurement.
//! 2. @b 3-Axis @b Gyroscope: Angular rate measurement with configurable sensitivity.
//! 3. @b Temperature @b Sensor: Integrated temperature measurement for calibration.
//! 4. @b I3C @b Interface: High-speed digital communication with CCC support.
//! 5. @b Dynamic @b Addressing: I3C SETDASA and RSTDAA command support.
//! 6. @b Configurable @b ODR: Multiple output data rates for power optimization.
//! 7. @b Multiple @b Full-Scale @b Ranges: Selectable sensitivity ranges for both sensors.
//! 8. @b DMA @b Support: Efficient data transfer with timeout and callback mechanisms.
//!
//! @section devices_i3c_lsm6dso_functionality Functionality
//!
//! - Initialize and configure LSM6DSO IMU sensor via I3C interface
//! - Read raw acceleration data from 3-axis accelerometer
//! - Read raw angular rate data from 3-axis gyroscope
//! - Read temperature data for sensor compensation
//! - Configure accelerometer and gyroscope output data rates and ranges
//! - Handle I3C Common Command Codes for device management
//! - Support dynamic I3C address assignment and reset operations
//! - Provide device identification and status monitoring
//! - Manage I3C read/write length limitations
//!
//! @section devices_i3c_lsm6dso_usage Usage
//!
//! 1. Initialize I3C host interface and configure I3C module
//! 2. Initialize LSM6DSO device with am_devices_i3c_lsm6dso_init()
//! 3. Configure sensor parameters (ODR, full-scale ranges) through register writes
//! 4. Read device ID with am_devices_i3c_lsm6dso_read_id() to verify communication
//! 5. Read acceleration data with am_devices_i3c_lsm6dso_acceleration_raw_get()
//! 6. Read gyroscope data with am_devices_i3c_lsm6dso_angular_rate_raw_get()
//! 7. Read temperature with am_devices_i3c_lsm6dso_temperature_raw_get()
//! 8. Monitor sensor status with am_devices_i3c_lsm6dso_get_status()
//! 9. Terminate device with am_devices_i3c_lsm6dso_term() when finished
//!
//! @section devices_i3c_lsm6dso_configuration Configuration
//!
//! - Set up I3C interface parameters and timing
//! - Configure accelerometer output data rate and full-scale range
//! - Configure gyroscope output data rate and full-scale range
//! - Set up I3C dynamic addressing and device provisioning
//! - Configure DMA parameters for efficient data transfer
//! - Set up interrupt and FIFO configurations as needed
//! - Optimize power consumption through ODR and operating mode selection
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
#include "am_devices_i3c_lsm6dso.h"
#include "am_util.h"

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************
typedef struct
{
    uint32_t                  ui32Module;
    void                      *pI3CHostHandle;
    bool                      bOccupied;

    uint8_t                   ui8StaticAddr;
    am_hal_i3c_device_type_e  eDeviceType;

    am_devices_i3c_lsm6dso_config_t sI3cLsm6dsoCfg;

} am_devices_i3c_lsm6dso_t;

am_devices_i3c_lsm6dso_t gAmI3cLsm6dso[AM_DEVICES_LSM6DSO_MAX_DEVICE_NUM];

//*****************************************************************************
//
//  DMA wait utility.
//
//*****************************************************************************
static uint32_t am_devices_i3c_lsm6dso_dma_wait(bool *pDMAComplete)
{
    uint32_t ui32Timeout = AM_DEVICES_LSM6DSO_DMA_TIMEOUT_CNT;
    //
    // Wait until DMA transfer done
    //
    while (!(*(volatile bool *)pDMAComplete))
    {
        if (ui32Timeout == 0)
        {
            break;
        }
        am_util_delay_us(1);
        am_util_debug_printf("waiting non-blocking complete\n");
        ui32Timeout--;
    }

    if ( ui32Timeout == 0 )
    {
        am_util_stdio_printf("Non-blocking timeout\n");
        return AM_DEVICES_LSM6DSO_STATUS_TIMEOUT;
    }

    return AM_DEVICES_LSM6DSO_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  lsm6dso devie data transfer based on the given sub address of sensor,
//  bytes number and transfer direction.
//
//*****************************************************************************
static uint32_t
am_devices_i3c_lsm6dso_xfer(void *pHandle,
                            uint8_t ui8SubAddr,
                            uint32_t *pData,
                            uint32_t ui32NumBytes,
                            bool bRead)
{
    am_hal_i3c_transfer_t Transaction;
    am_devices_i3c_lsm6dso_t *pDevHandle = (am_devices_i3c_lsm6dso_t *)pHandle;
    am_devices_i3c_lsm6dso_config_t *pDeviceCfg = &pDevHandle->sI3cLsm6dsoCfg;

    if ( !pHandle )
    {
        return AM_DEVICES_LSM6DSO_STATUS_INVALID_HANDLE;
    }

    //
    // Create the transaction
    //

    Transaction.Device.ui8DynamicAddr = pDeviceCfg->ui8DynamicAddr;
    Transaction.Device.eDeviceType    = AM_HAL_I3C_DEVICE_I3C;
    Transaction.eSpeedMode            = pDeviceCfg->eSpeedMode;

    Transaction.ui32NumBytes  = ui32NumBytes;
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

        *pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;
        if ( am_hal_i3c_nonblocking_transfer(pDevHandle->pI3CHostHandle, &Transaction)  != AM_HAL_STATUS_SUCCESS )
        {
            return AM_DEVICES_LSM6DSO_STATUS_ERROR;
        }
        return pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback(pDeviceCfg->sI3cDmaCfg.pDMAComplete);
    }
    else
    {
        if ( am_hal_i3c_blocking_transfer(pDevHandle->pI3CHostHandle, &Transaction) != AM_HAL_STATUS_SUCCESS)
        {
            return AM_DEVICES_LSM6DSO_STATUS_ERROR;
        }
    }

    return AM_DEVICES_LSM6DSO_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Reads the device id of the sensor.
//
//*****************************************************************************
static uint32_t
am_devices_i3c_lsm6dso_read_id(void *pHandle, uint32_t *pDeviceID)
{
    if ( !pHandle )
    {
        return AM_DEVICES_LSM6DSO_STATUS_INVALID_HANDLE;
    }

    if ( !pDeviceID )
    {
        return AM_DEVICES_LSM6DSO_STATUS_INVALID_ARG;
    }

    return am_devices_i3c_lsm6dso_xfer(pHandle, AM_DEVICES_LSM6DSO_WHO_AMI_I, pDeviceID, 1, true);
}

//*****************************************************************************
//
//  Device configuration initialization
//
//*****************************************************************************
static uint32_t
am_devices_i3c_lsm6dso_device_init(void *pHandle)
{
    uint32_t ui32Status = AM_DEVICES_LSM6DSO_STATUS_SUCCESS;
    uint16_t ui16Temp __attribute__((aligned(4)));

    lsm6dso_ctrl3_c_t reg3 __attribute__((aligned(4))) = {0};
    //
    // Lsm6dso software reset, restore the default values in user registers.[set]
    //
    if ( !am_devices_i3c_lsm6dso_read_reg(pHandle, AM_DEVICES_LSM6DSO_CTRL_REG3, (uint32_t *)&reg3, sizeof(lsm6dso_ctrl3_c_t)) )
    {
        reg3.sw_reset = 1;
        reg3.if_inc = 1;
        ui16Temp = *(uint8_t *)&reg3 << 8;
        ui32Status = am_devices_i3c_lsm6dso_write_reg(pHandle, AM_DEVICES_LSM6DSO_CTRL_REG3, (uint32_t *)&ui16Temp, sizeof(lsm6dso_ctrl3_c_t));
        if ( AM_DEVICES_LSM6DSO_STATUS_SUCCESS != ui32Status )
        {
            am_util_stdio_printf("Failed read lsm6dso reg:0x%x!\n", AM_DEVICES_LSM6DSO_CTRL_REG3);
            return ui32Status;
        }
    }

    //
    // Get Lsm6dso software reset status
    //
    uint32_t ui32Timeout = 200;
    do
    {
        ui32Status = am_devices_i3c_lsm6dso_read_reg(pHandle, AM_DEVICES_LSM6DSO_CTRL_REG3, (uint32_t *)&reg3, sizeof(lsm6dso_ctrl3_c_t));
        ui32Timeout--;

        if ( ui32Timeout == 0 )
        {
            am_util_stdio_printf("Lsm6dso software reset timeout\n");
            return AM_DEVICES_LSM6DSO_STATUS_TIMEOUT;
        }
        am_util_delay_ms(1);
    }
    while( reg3.sw_reset & 1 );

    lsm6dso_ctrl1_xl_t reg1 = {0};
    reg1.odr_xl = AM_DEVICES_LSM6DSO_ODR_52Hz;
    ui16Temp = *(uint8_t *)&reg1 << 8;
    ui32Status = am_devices_i3c_lsm6dso_write_reg(pHandle, AM_DEVICES_LSM6DSO_CTRL_REG1, (uint32_t *)&ui16Temp, sizeof(lsm6dso_ctrl1_xl_t));
    if ( ui32Status )
    {
        am_util_stdio_printf("Failed write lsm6dso reg:0x%x!\n", AM_DEVICES_LSM6DSO_CTRL_REG1);
        return ui32Status;
    }

    lsm6dso_ctrl2_g_t reg2 = {0};
    reg2.odr_g = AM_DEVICES_LSM6DSO_ODR_52Hz;
    ui16Temp = *(uint8_t *)&reg2 << 8;
    ui32Status = am_devices_i3c_lsm6dso_write_reg(pHandle, AM_DEVICES_LSM6DSO_CTRL_REG2, (uint32_t *)&ui16Temp, sizeof(lsm6dso_ctrl1_xl_t));
    if ( ui32Status )
    {
        am_util_stdio_printf("Failed write lsm6dso reg:0x%x!\n", AM_DEVICES_LSM6DSO_CTRL_REG2);
        return ui32Status;
    }

    reg3.bdu = 1;
    ui16Temp = *(uint8_t *)&reg3 << 8;
    ui32Status = am_devices_i3c_lsm6dso_write_reg(pHandle, AM_DEVICES_LSM6DSO_CTRL_REG3, (uint32_t *)&ui16Temp, sizeof(lsm6dso_ctrl3_c_t));
    if ( ui32Status )
    {
        am_util_stdio_printf("Failed write lsm6dso reg:0x%x!\n", AM_DEVICES_LSM6DSO_CTRL_REG3);
        return ui32Status;
    }

    return ui32Status;
}

uint32_t
am_devices_i3c_lsm6dso_send_cccs(void *pHandle, uint32_t *pui32Data, uint8_t ui8CccId)
{
    am_devices_i3c_lsm6dso_t *pDevHandle = (am_devices_i3c_lsm6dso_t *)pHandle;
    am_devices_i3c_lsm6dso_config_t *pDeviceCfg = &pDevHandle->sI3cLsm6dsoCfg;
    uint32_t ui32Status = AM_DEVICES_LSM6DSO_STATUS_SUCCESS;

    if ( !pHandle )
    {
        return AM_DEVICES_LSM6DSO_STATUS_INVALID_HANDLE;
    }

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
        *pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;
        Transaction.pui32CmdRingBuf    = pDeviceCfg->sI3cDmaCfg.pCmdRingBuf;
        Transaction.ui32CmdRingBufLen  = pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen;
        Transaction.pui32RespRingBuf   = pDeviceCfg->sI3cDmaCfg.pRespRingBuf;
        Transaction.ui32RespRingBufLen = pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen;
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
            return AM_DEVICES_LSM6DSO_STATUS_INVALID_ARG;
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
        ui32Status = am_hal_i3c_do_daa(pDevHandle->pI3CHostHandle, &Transaction);
    }
    else
    {
        ui32Status = am_hal_i3c_send_ccc_cmd(pDevHandle->pI3CHostHandle, &Transaction);
    }

    //
    // Wait until DMA transfer done
    //
    if ( pDeviceCfg->eTransferMode == AM_HAL_I3C_XFER_DMA )
    {
        return pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback(pDeviceCfg->sI3cDmaCfg.pDMAComplete);
    }

    return ui32Status;
}

uint32_t
am_devices_i3c_lsm6dso_write_reg(void *pHandle, uint8_t ui8WriteAddr,
                                 uint32_t *pui32Data, uint32_t ui32NumBytes)
{
    if ( !pHandle )
    {
        return AM_DEVICES_LSM6DSO_STATUS_INVALID_HANDLE;
    }

    if ( !pui32Data )
    {
        return AM_DEVICES_LSM6DSO_STATUS_INVALID_ARG;
    }

    return am_devices_i3c_lsm6dso_xfer(pHandle, ui8WriteAddr, pui32Data, ui32NumBytes, false);
}

uint32_t
am_devices_i3c_lsm6dso_read_reg(void *pHandle, uint8_t ui8ReadAddr,
                                uint32_t *pui32Data, uint32_t ui32NumBytes)
{
    if ( !pHandle )
    {
        return AM_DEVICES_LSM6DSO_STATUS_INVALID_HANDLE;
    }

    if ( !pui32Data )
    {
        return AM_DEVICES_LSM6DSO_STATUS_INVALID_ARG;
    }

    return am_devices_i3c_lsm6dso_xfer(pHandle, ui8ReadAddr, pui32Data, ui32NumBytes, true);
}

uint32_t
am_devices_i3c_lsm6dso_init(uint32_t ui32Module, void *pHandle, void **ppDevHandle,
                            am_devices_i3c_lsm6dso_config_t *pDevConfig)
{
    uint32_t ui32Index  = 0;
    uint32_t ui32DeviceId = 0;
    uint32_t ui32Status = AM_DEVICES_LSM6DSO_STATUS_SUCCESS;
    am_devices_i3c_lsm6dso_config_t *pDeviceCfg;
    am_devices_i3c_lsm6dso_t *pDevHandle;

    //
    // Allocate a vacant device handle
    //
    for ( ui32Index = 0; ui32Index < AM_DEVICES_LSM6DSO_MAX_DEVICE_NUM; ui32Index++ )
    {
        if ( gAmI3cLsm6dso[ui32Index].bOccupied == false )
        {
            pDevHandle = &gAmI3cLsm6dso[ui32Index];
            pDeviceCfg = &gAmI3cLsm6dso[ui32Index].sI3cLsm6dsoCfg;
            break;
        }
    }

    if ( ui32Index == AM_DEVICES_LSM6DSO_MAX_DEVICE_NUM )
    {
        return AM_DEVICES_LSM6DSO_STATUS_OUT_OF_RANGE;
    }

    if ( (ui32Module > AM_REG_I3C_NUM_MODULES) || (pDevConfig == NULL) )
    {
        return AM_DEVICES_LSM6DSO_STATUS_INVALID_HANDLE;
    }

#if defined(apollo510L_eb)
    pDevHandle->ui8StaticAddr      = AM_DEVICES_LSM6DSO_STATIC_ADDR1;
#elif defined(apollo510L_val)
    pDevHandle->ui8StaticAddr      = AM_DEVICES_LSM6DSO_STATIC_ADDR2;
#else
    pDevHandle->ui8StaticAddr      = AM_DEVICES_LSM6DSO_STATIC_ADDR1;
#endif

    pDevHandle->eDeviceType        = AM_HAL_I3C_DEVICE_I3C;
    pDevHandle->ui32Module         = ui32Module;
    pDevHandle->pI3CHostHandle     = pHandle;

    if ( pDevHandle->eDeviceType == AM_HAL_I3C_DEVICE_I3C )
    {
        pDeviceCfg->eTransferMode  = pDevConfig->eTransferMode;
        pDeviceCfg->eSpeedMode     = pDevConfig->eSpeedMode;
        pDeviceCfg->ui8DynamicAddr = pDevConfig->ui8DynamicAddr;

        if ( pDevConfig->eTransferMode == AM_HAL_I3C_XFER_DMA )
        {
            if ( pDevConfig->sI3cDmaCfg.pDMAComplete == NULL )
            {
                return AM_DEVICES_LSM6DSO_STATUS_INVALID_ARG;
            }

            if ( pDevConfig->sI3cDmaCfg.pfnI3cTimeoutCallback == NULL )
            {
                pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback = am_devices_i3c_lsm6dso_dma_wait;
            }
            else
            {
                pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback = pDevConfig->sI3cDmaCfg.pfnI3cTimeoutCallback;
            }

            pDeviceCfg->sI3cDmaCfg.pDMAComplete       = pDevConfig->sI3cDmaCfg.pDMAComplete;
            pDeviceCfg->sI3cDmaCfg.pCmdRingBuf        = pDevConfig->sI3cDmaCfg.pCmdRingBuf;
            pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen  = pDevConfig->sI3cDmaCfg.ui32CmdRingBufLen;
            pDeviceCfg->sI3cDmaCfg.pRespRingBuf       = pDevConfig->sI3cDmaCfg.pRespRingBuf;
            pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen = pDevConfig->sI3cDmaCfg.ui32RespRingBufLen;
        }

        //
        // Send RSTDAA ccc to sensor
        //
        uint32_t ui32CmdData = 0;
        ui32Status = am_devices_i3c_lsm6dso_send_cccs(pDevHandle, &ui32CmdData, AM_HAL_I3C_CCC_RSTDAA(true));
        if ( AM_DEVICES_LSM6DSO_STATUS_SUCCESS != ui32Status )
        {
            am_util_stdio_printf("Failed to reset lsm6dso dasa!\n");
            return ui32Status;
        }

        //
        // Send SETDASA ccc to set the dynamic address of sensor
        //
        ui32Status = am_devices_i3c_lsm6dso_send_cccs(pDevHandle, &ui32CmdData, AM_HAL_I3C_CCC_SETDASA);
        if ( AM_DEVICES_LSM6DSO_STATUS_SUCCESS != ui32Status )
        {
            am_util_stdio_printf("Failed to set lsm6dso dasa!\n");
            return ui32Status;
        }

        //
        // Read device ID
        //
        ui32Status = am_devices_i3c_lsm6dso_read_id(pDevHandle, &ui32DeviceId);
        if ( AM_DEVICES_LSM6DSO_STATUS_SUCCESS != ui32Status )
        {
            am_util_stdio_printf("Failed to read device id!\n");
            return ui32Status;
        }

        if ( (ui32DeviceId & 0xFF) == AM_DEVICES_LSM6DSO_DEVICE_ID )
        {
            am_util_stdio_printf("Get lsm6dso device id:0x%x!\n", ui32DeviceId);
        }
        else
        {
            am_util_stdio_printf("Get lsm6dso device id failed!\n");
            return AM_DEVICES_LSM6DSO_STATUS_ERROR;
        }

        ui32Status = am_devices_i3c_lsm6dso_device_init(pDevHandle);
        if ( ui32Status )
        {
            return ui32Status;
        }

    }
    else if ( pDevHandle->eDeviceType == AM_HAL_I3C_DEVICE_I2C )
    {
        //
        // TODO: legacy I2C driver support
        //
        return AM_DEVICES_LSM6DSO_STATUS_ERROR;
    }

    pDevHandle->bOccupied = true;

    *ppDevHandle = pDevHandle;

    return AM_DEVICES_LSM6DSO_STATUS_SUCCESS;
}

uint32_t
am_devices_i3c_lsm6dso_term(void *pHandle)
{
    am_devices_i3c_lsm6dso_t *pDevHandle = (am_devices_i3c_lsm6dso_t *)pHandle;
    am_devices_i3c_lsm6dso_config_t *pDeviceCfg = &pDevHandle->sI3cLsm6dsoCfg;
    uint32_t ui32Status = AM_DEVICES_LSM6DSO_STATUS_SUCCESS;

    //
    // Send RSTDAA ccc to ensure all device'Dynamic Address cleared
    //
    uint32_t ui32CmdData = 0;
    ui32Status = am_devices_i3c_lsm6dso_send_cccs(pDevHandle, &ui32CmdData, AM_HAL_I3C_CCC_RSTDAA(true));
    if ( AM_DEVICES_LSM6DSO_STATUS_SUCCESS != ui32Status )
    {
        am_util_stdio_printf("Failed to reset lsm6dso dasa!\n");
        return ui32Status;
    }


    //
    // Clear conditional storage
    //
    memset(&pDeviceCfg->sI3cDmaCfg, 0, sizeof(am_devices_i3c_lsm6dso_dma_config_t));

    //
    // Free this device handle
    //
    pDevHandle->bOccupied = false;

    //
    // Return the status.
    //
    return AM_DEVICES_LSM6DSO_STATUS_SUCCESS;
}

uint32_t
am_devices_i3c_lsm6dso_get_status(void *pHandle, lsm6dso_status_reg_t *pData)
{
    return am_devices_i3c_lsm6dso_read_reg(pHandle, AM_DEVICES_LSM6DSO_STATUS, (uint32_t *)pData, sizeof(lsm6dso_status_reg_t));
}

uint32_t
am_devices_i3c_lsm6dso_acceleration_raw_get(void *pHandle, int16_t *pui16Data)
{
    uint8_t buff[6] __attribute__((aligned(4)));

    if ( am_devices_i3c_lsm6dso_read_reg(pHandle, AM_DEVICES_LSM6DSO_OUTX_L_A, (uint32_t *)buff, 6) )
    {
        return AM_DEVICES_LSM6DSO_STATUS_ERROR;
    }

    pui16Data[0] = (int16_t)buff[1];
    pui16Data[0] = (pui16Data[0] * 256) + (int16_t)buff[0];
    pui16Data[1] = (int16_t)buff[3];
    pui16Data[1] = (pui16Data[1] * 256) + (int16_t)buff[2];
    pui16Data[2] = (int16_t)buff[5];
    pui16Data[2] = (pui16Data[2] * 256) + (int16_t)buff[4];

    return AM_DEVICES_LSM6DSO_STATUS_SUCCESS;
}

uint32_t
am_devices_i3c_lsm6dso_angular_rate_raw_get(void *pHandle, int16_t *pui16Data)
{
    uint8_t buff[6] __attribute__((aligned(4)));

    if ( am_devices_i3c_lsm6dso_read_reg(pHandle, AM_DEVICES_LSM6DSO_OUTX_L_G, (uint32_t *)buff, 6) )
    {
        return AM_DEVICES_LSM6DSO_STATUS_ERROR;
    }

    pui16Data[0] = (int16_t)buff[1];
    pui16Data[0] = (pui16Data[0] * 256) + (int16_t)buff[0];
    pui16Data[1] = (int16_t)buff[3];
    pui16Data[1] = (pui16Data[1] * 256) + (int16_t)buff[2];
    pui16Data[2] = (int16_t)buff[5];
    pui16Data[2] = (pui16Data[2] * 256) + (int16_t)buff[4];

    return AM_DEVICES_LSM6DSO_STATUS_SUCCESS;
}

uint32_t
am_devices_i3c_lsm6dso_temperature_raw_get(void *pHandle, int16_t *pui16Data)
{
    uint8_t buff[2] __attribute__((aligned(4)));

    if ( am_devices_i3c_lsm6dso_read_reg(pHandle, AM_DEVICES_LSM6DSO_OUT_TEMP_L, (uint32_t *)buff, 2) )
    {
        return AM_DEVICES_LSM6DSO_STATUS_ERROR;
    }

    pui16Data[0] = (int16_t)buff[1];
    pui16Data[0] = (pui16Data[0] * 256) + (int16_t)buff[0];

    return AM_DEVICES_LSM6DSO_STATUS_SUCCESS;
}

uint32_t
am_devices_i3c_lsm6dso_write_len_set(void *pHandle, uint16_t ui16WriteLen)
{
    //
    // Set lsm6dso max write length direct ccc
    //
    return am_devices_i3c_lsm6dso_send_cccs(pHandle, (uint32_t *)&ui16WriteLen, AM_HAL_I3C_CCC_SETMWL(false));
}

uint32_t
am_devices_i3c_lsm6dso_read_len_set(void *pHandle, uint16_t ui16ReadLen)
{
    //
    // Set lsm6dso max read length direct ccc
    //
    return am_devices_i3c_lsm6dso_send_cccs(pHandle, (uint32_t *)&ui16ReadLen, AM_HAL_I3C_CCC_SETMRL(false));
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
