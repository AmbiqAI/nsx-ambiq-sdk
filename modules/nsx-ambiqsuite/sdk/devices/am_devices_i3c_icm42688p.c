//*****************************************************************************
//
//! @file am_devices_i3c_icm42688p.c
//!
//! @brief Driver for the TDK InvenSense ICM-42688-P 6-axis IMU
//!        (accelerometer, gyroscope, and temperature sensor)
//!
//! @addtogroup icm42688p I3C Device Control for ICM-42688-P sensor
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
#include <string.h>
#include <stdlib.h>
#include "am_mcu_apollo.h"
#include "am_devices_i3c_icm42688p.h"
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
    am_devices_i3c_icm42688p_config_t sI3cIcm42688pCfg;
} am_devices_i3c_icm42688p_t;

am_devices_i3c_icm42688p_t         gAmI3cIcm42688p[AM_DEVICES_ICM42688P_MAX_DEVICE_NUM];

//*****************************************************************************
//
//  ICM-42688-P device driver default callback to wait DMA complete
//
//*****************************************************************************
static uint32_t
am_devices_i3c_icm42688p_timeout_callback(bool *pDMAComplete)
{
    uint32_t ui32Timeout = AM_DEVICES_ICM42688P_DMA_TIMEOUT_CNT;
    //
    // Wait until DMA transfer done
    //
    while (!(*(volatile bool *)pDMAComplete))
    {
        if (ui32Timeout == 0)
        {
            break;
        }
        am_util_delay_us(AM_DEVICES_ICM42688P_DMA_DELAY_US);
        am_util_debug_printf("waiting non-blocking complete\n");
        ui32Timeout--;
    }

    if (ui32Timeout == 0)
    {
        am_util_stdio_printf("Non-blocking timeout\n");
        return AM_DEVICES_ICM42688P_STATUS_TIMEOUT;
    }

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  ICM-42688-P device data transfer based on the given sub address of sensor,
//  bytes number and transfer direction.
//
//*****************************************************************************
static uint32_t
am_devices_i3c_icm42688p_xfer(void *pHandle,
                              uint8_t ui8SubAddr,
                              uint32_t *pData,
                              uint32_t ui32NumBytes,
                              bool bRead)
{
    uint32_t ui32Status;
    am_hal_i3c_transfer_t Transaction;
    am_devices_i3c_icm42688p_t *pDevHandle = (am_devices_i3c_icm42688p_t *)pHandle;
    am_devices_i3c_icm42688p_config_t *pDeviceCfg = &pDevHandle->sI3cIcm42688pCfg;

    if (!pHandle)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE;
    }

    //
    // Create the transaction
    //
    Transaction.Device.ui8DynamicAddr = pDeviceCfg->ui8DynamicAddr;
    Transaction.Device.eDeviceType = AM_HAL_I3C_DEVICE_I3C;
    Transaction.ui32NumBytes  = ui32NumBytes;
    Transaction.eSpeedMode    = pDeviceCfg->eSpeedMode;
    Transaction.bCombo16bitOfs = false;
    Transaction.ui16ComboOfs  = ui8SubAddr;

    if (bRead)
    {
        Transaction.bComboXfer    = true;
        Transaction.eDirection    = AM_HAL_I3C_DIR_READ;
        Transaction.pui32RxBuffer = pData;

        if (Transaction.eSpeedMode == AM_HAL_I3C_HDR_DDR)
        {
            Transaction.bComboXfer = true;
            Transaction.bCombo16bitOfs = true;
            Transaction.ui8CCCID = 0x80;
            Transaction.ui32NumBytes = ui32NumBytes;
        }
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
        Transaction.pui32TxBuffer = pData;

        if (Transaction.eSpeedMode == AM_HAL_I3C_HDR_DDR)
        {
            //
            // Match atomiq110 widget HDR-DDR write buffer layout:
            //   [0] = sub-address, [1] = HDR cmd (0x00), [2..] = data
            //   combo ofs = 0, byte count = data length + 2
            //
            memmove(&pui8Buffer[2], &pui8Buffer[1], ui32NumBytes);
            pui8Buffer[1] = 0x00;
            Transaction.ui8CCCID = 0;
            Transaction.ui16ComboOfs = 0;
            Transaction.ui32NumBytes = ui32NumBytes + 2;
        }
        else
        {
            Transaction.ui32NumBytes = ui32NumBytes + 1;
        }
    }

    //
    // Execute the transaction over I3C.
    //
    if (pDeviceCfg->eTransferMode == AM_HAL_I3C_XFER_DMA)
    {
        Transaction.pui32CmdRingBuf     = pDeviceCfg->sI3cDmaCfg.pCmdRingBuf;
        Transaction.ui32CmdRingBufLen   = pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen;
        Transaction.pui32RespRingBuf    = pDeviceCfg->sI3cDmaCfg.pRespRingBuf;
        Transaction.ui32RespRingBufLen  = pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen;

        *(volatile bool *)pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;

        if (am_hal_i3c_nonblocking_transfer(pDevHandle->pI3cHandle, &Transaction)  != AM_HAL_STATUS_SUCCESS)
        {
            return AM_DEVICES_ICM42688P_STATUS_ERROR;
        }

        //
        // Wait until DMA transfer done
        //
        ui32Status = pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback(pDeviceCfg->sI3cDmaCfg.pDMAComplete);
        if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
        {
            return ui32Status;
        }
    }
    else
    {
        if (am_hal_i3c_blocking_transfer(pDevHandle->pI3cHandle, &Transaction) != AM_HAL_STATUS_SUCCESS)
        {
            return AM_DEVICES_ICM42688P_STATUS_ERROR;
        }
    }

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Reads the device id of the sensor.
//
//*****************************************************************************
static uint32_t
am_devices_i3c_icm42688p_read_id(void *pHandle, uint32_t *pDeviceID)
{
    if (!pHandle)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE;
    }

    if (!pDeviceID)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_ARG;
    }

    return am_devices_i3c_icm42688p_xfer(pHandle, AM_DEVICES_ICM42688P_WHO_AM_I, pDeviceID, 1, true);
}

//*****************************************************************************
//
//! @brief Send I3C Common Command Codes
//!
//! @param pHandle   - Pointer to device handle
//! @param pui32Data - Buffer to config sensor or save current configuration
//! @param ui8CccId  - Common command code index
//!
//! This function sends I3C CCCs to sensor to configure or get sensor information
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_i3c_icm42688p_send_cccs(void *pHandle, uint32_t *pui32Data, uint8_t ui8CccId)
{
    uint32_t ui32Status = AM_DEVICES_ICM42688P_STATUS_SUCCESS;

    if (!pHandle)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE;
    }

    am_devices_i3c_icm42688p_t *pDevHandle = (am_devices_i3c_icm42688p_t *)pHandle;
    am_devices_i3c_icm42688p_config_t *pDeviceCfg = &pDevHandle->sI3cIcm42688pCfg;
    am_hal_i3c_transfer_t Transaction;

    Transaction.Device.eDeviceType = AM_HAL_I3C_DEVICE_I3C;
    Transaction.eSpeedMode     = pDeviceCfg->eSpeedMode;
    Transaction.ui8CCCID       = ui8CccId;
    Transaction.bComboXfer     = false;
    Transaction.bCombo16bitOfs = false;
    Transaction.ui16ComboOfs   = 0;

    Transaction.Device.ui8DynamicAddr = pDeviceCfg->ui8DynamicAddr;
    Transaction.Device.ui8StaticAddr  = pDevHandle->ui8StaticAddr;

    if (pDeviceCfg->eTransferMode == AM_HAL_I3C_XFER_DMA)
    {
        *(volatile bool *)pDeviceCfg->sI3cDmaCfg.pDMAComplete = false;
        Transaction.pui32CmdRingBuf     = pDeviceCfg->sI3cDmaCfg.pCmdRingBuf;
        Transaction.ui32CmdRingBufLen   = pDeviceCfg->sI3cDmaCfg.ui32CmdRingBufLen;
        Transaction.pui32RespRingBuf    = pDeviceCfg->sI3cDmaCfg.pRespRingBuf;
        Transaction.ui32RespRingBufLen  = pDeviceCfg->sI3cDmaCfg.ui32RespRingBufLen;
    }

    switch (ui8CccId)
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
            return AM_DEVICES_ICM42688P_STATUS_INVALID_ARG;
    }

    if (Transaction.eDirection == AM_HAL_I3C_DIR_WRITE)
    {
        Transaction.pui32TxBuffer = pui32Data;
    }
    else
    {
        Transaction.pui32RxBuffer = pui32Data;
    }

    if (Transaction.ui8CCCID == AM_HAL_I3C_CCC_ENTDAA || Transaction.ui8CCCID == AM_HAL_I3C_CCC_SETDASA)
    {
        ui32Status = am_hal_i3c_do_daa(pDevHandle->pI3cHandle, &Transaction);
    }
    else
    {
        ui32Status = am_hal_i3c_send_ccc_cmd(pDevHandle->pI3cHandle, &Transaction);
    }

    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_ICM42688P_STATUS_ERROR;
    }

    if (pDeviceCfg->eTransferMode == AM_HAL_I3C_XFER_DMA)
    {
        //
        // Wait until DMA transfer done
        //
        ui32Status = pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback(pDeviceCfg->sI3cDmaCfg.pDMAComplete);
        if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
        {
            return ui32Status;
        }
    }

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

//
//! @brief Select ICM-42688-P register bank.
//!
//! @param pHandle  - Pointer to device handle
//! @param ui32Bank - Bank number (e.g. AM_DEVICES_ICM42688P_BANKx)
//!
//! write_reg() places the register address in buffer byte0, so the bank
//! value is shifted into byte1 before the transfer.
//!
//! @return Status.
//
static uint32_t
am_devices_i3c_icm42688p_bank_select(void *pHandle, uint32_t ui32Bank)
{
    uint32_t ui32RegData = (ui32Bank & 0xFFU) << 8;

    return am_devices_i3c_icm42688p_write_reg(pHandle,
                                              AM_DEVICES_ICM42688P_BANK_SEL,
                                              &ui32RegData,
                                              1);
}

static uint32_t
am_devices_i3c_icm42688p_device_init(void *pHandle)
{
    uint32_t ui32RegData = 0;
    uint32_t ui32Status = AM_DEVICES_ICM42688P_STATUS_SUCCESS;

    //
    // Choose bank0 for device configuration
    //
    ui32Status = am_devices_i3c_icm42688p_bank_select(pHandle, AM_DEVICES_ICM42688P_BANK0);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        am_util_stdio_printf("Failed to select ICM-42688-P bank region:0x%x!\n", ui32Status);
        return ui32Status;
    }

    ui32Status = am_devices_i3c_icm42688p_read_reg(pHandle, AM_DEVICES_ICM42688P_PWR_MGMT0, &ui32RegData, 1);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        am_util_stdio_printf("Failed to read ICM-42688-P reg:0x%x!\n", AM_DEVICES_ICM42688P_PWR_MGMT0);
        return ui32Status;
    }
    else
    {
        ui32RegData |= AM_DEVICES_ICM42688P_ACCEL_ENABLE_MSK | AM_DEVICES_ICM42688P_GYRO_ENABLE_MSK;
        ui32RegData = ui32RegData << 8;
        ui32Status = am_devices_i3c_icm42688p_write_reg(pHandle, AM_DEVICES_ICM42688P_PWR_MGMT0, &ui32RegData, 1);
        if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
        {
            am_util_stdio_printf("Failed write ICM-42688-P reg:0x%x!\n", AM_DEVICES_ICM42688P_PWR_MGMT0);
            return ui32Status;
        }
    }

    am_util_delay_ms(50);

    ui32RegData = AM_DEVICES_ICM42688P_GYRO_FS_SEL_250 | AM_DEVICES_ICM42688P_GYRO_ODR_50HZ;
    ui32RegData = ui32RegData << 8;
    ui32Status = am_devices_i3c_icm42688p_write_reg(pHandle, AM_DEVICES_ICM42688P_GYRO_CONFIG0, &ui32RegData, 1);
    if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Failed write ICM-42688-P reg:0x%x!\n", AM_DEVICES_ICM42688P_GYRO_CONFIG0);
        return ui32Status;
    }

    ui32RegData = AM_DEVICES_ICM42688P_ACCEL_FS_SEL_2G | AM_DEVICES_ICM42688P_ACCEL_ODR_50HZ;
    ui32RegData = ui32RegData << 8;
    ui32Status = am_devices_i3c_icm42688p_write_reg(pHandle, AM_DEVICES_ICM42688P_ACCEL_CONFIG0, &ui32RegData, 1);
    if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Failed write ICM-42688-P reg:0x%x!\n", AM_DEVICES_ICM42688P_ACCEL_CONFIG0);
        return ui32Status;
    }

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Enter ICM-42688-P HDR-DDR mode (same sequence as i3c_hdr_ddr_test_cases).
//
//*****************************************************************************
uint32_t
am_devices_i3c_icm42688p_enter_hdr_mode(void *pHandle)
{
    uint32_t ui32Status = AM_DEVICES_ICM42688P_STATUS_SUCCESS;
    uint32_t ui32RegData = 0;
    am_devices_i3c_icm42688p_t *pDevHandle = (am_devices_i3c_icm42688p_t *)pHandle;
    am_hal_i3c_speed_mode_e eSavedSpeed;

    if (!pHandle)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE;
    }

    //
    // Program device registers in SDR before switching to HDR-DDR.
    //
    eSavedSpeed = pDevHandle->sI3cIcm42688pCfg.eSpeedMode;
    pDevHandle->sI3cIcm42688pCfg.eSpeedMode = AM_HAL_I3C_SDR0;

    //
    // Select bank4 (HDR CFG register lives in bank4 in the test flow).
    //
    ui32Status = am_devices_i3c_icm42688p_bank_select(pHandle, AM_DEVICES_ICM42688P_BANK4);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        pDevHandle->sI3cIcm42688pCfg.eSpeedMode = eSavedSpeed;
        return ui32Status;
    }

    //
    // RMW reg 0x52: clear bit4 (0x10), matching HDR test case.
    //
    ui32Status = am_devices_i3c_icm42688p_read_reg(pHandle,
                                                   AM_DEVICES_ICM42688P_HDR_CFG_REG,
                                                   &ui32RegData,
                                                   1);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        pDevHandle->sI3cIcm42688pCfg.eSpeedMode = eSavedSpeed;
        return ui32Status;
    }

    ui32RegData = (ui32RegData & 0xFFU) & ~(uint32_t)AM_DEVICES_ICM42688P_I3C_ENABLE_MSK;
    ui32RegData = ui32RegData << 8;
    ui32Status = am_devices_i3c_icm42688p_write_reg(pHandle,
                                                    AM_DEVICES_ICM42688P_HDR_CFG_REG,
                                                    &ui32RegData,
                                                    1);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        pDevHandle->sI3cIcm42688pCfg.eSpeedMode = eSavedSpeed;
        return ui32Status;
    }

    //
    // Restore bank0 for normal sensor register access.
    //
    ui32Status = am_devices_i3c_icm42688p_bank_select(pHandle, AM_DEVICES_ICM42688P_BANK0);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        pDevHandle->sI3cIcm42688pCfg.eSpeedMode = eSavedSpeed;
        return ui32Status;
    }

    //
    // Subsequent transfers run in HDR-DDR.
    //
    pDevHandle->sI3cIcm42688pCfg.eSpeedMode = AM_HAL_I3C_HDR_DDR;

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Exit ICM-42688-P HDR-DDR mode (reverse of enter_hdr_mode).
//
//*****************************************************************************
uint32_t
am_devices_i3c_icm42688p_exit_hdr_mode(void *pHandle)
{
    uint32_t ui32Status = AM_DEVICES_ICM42688P_STATUS_SUCCESS;
    uint32_t ui32RegData = 0;
    am_devices_i3c_icm42688p_t *pDevHandle = (am_devices_i3c_icm42688p_t *)pHandle;

    if (!pHandle)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE;
    }

    //
    // Program device registers in SDR0 before leaving HDR-DDR.
    //
    pDevHandle->sI3cIcm42688pCfg.eSpeedMode = AM_HAL_I3C_SDR0;

    //
    // Select bank4 and restore HDR CFG bit4 (I3C_ENABLE_MSK).
    //
    ui32Status = am_devices_i3c_icm42688p_bank_select(pHandle, AM_DEVICES_ICM42688P_BANK4);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    ui32Status = am_devices_i3c_icm42688p_read_reg(pHandle,
                                                   AM_DEVICES_ICM42688P_HDR_CFG_REG,
                                                   &ui32RegData,
                                                   1);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    ui32RegData = (ui32RegData & 0xFFU) | (uint32_t)AM_DEVICES_ICM42688P_I3C_ENABLE_MSK;
    ui32RegData = ui32RegData << 8;
    ui32Status = am_devices_i3c_icm42688p_write_reg(pHandle,
                                                    AM_DEVICES_ICM42688P_HDR_CFG_REG,
                                                    &ui32RegData,
                                                    1);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Restore bank0 for normal sensor register access.
    //
    ui32Status = am_devices_i3c_icm42688p_bank_select(pHandle, AM_DEVICES_ICM42688P_BANK0);
    if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Initialize the ICM-42688-P driver.
//!
//! @param ui32Module  - I3C instance
//! @param pI3cHandle  - I3C controller handle
//! @param ppDevHandle - Device handle which needs to be returned
//! @param pDevConfig  - I3C device structure describing the target sensor
//!
//! @note This function should be called before any other am_devices_i3c_icm42688p
//! functions. It is used to tell the other functions how to communicate
//! with the I3C sensors.
//!
//! @return Status.
//
//*****************************************************************************
uint32_t
am_devices_i3c_icm42688p_init(uint32_t ui32Module,
                              void *pI3cHandle,
                              void **ppDevHandle,
                              am_devices_i3c_icm42688p_config_t *pDevConfig)
{
    uint32_t ui32Index  = 0;
    uint32_t ui32DeviceId = 0;
    uint32_t ui32Status = AM_DEVICES_ICM42688P_STATUS_SUCCESS;

    am_devices_i3c_icm42688p_config_t *pDeviceCfg;
    am_devices_i3c_icm42688p_t *pDevHandle;

    //
    // Allocate a vacant device handle
    //
    for (ui32Index = 0; ui32Index < AM_DEVICES_ICM42688P_MAX_DEVICE_NUM; ui32Index++)
    {
        if (gAmI3cIcm42688p[ui32Index].bOccupied == false)
        {
            pDevHandle = &gAmI3cIcm42688p[ui32Index];
            pDeviceCfg = &gAmI3cIcm42688p[ui32Index].sI3cIcm42688pCfg;
            break;
        }
    }

    if (ui32Index == AM_DEVICES_ICM42688P_MAX_DEVICE_NUM)
    {
        return AM_DEVICES_ICM42688P_STATUS_OUT_OF_RANGE;
    }

    if (pDevConfig == NULL)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_ARG;
    }

    if (ui32Module >= AM_REG_I3C_NUM_MODULES)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE;
    }

#ifdef I3C_CLOCK_DIV_ENABLE
    //
    // Configure I3C clock divider
    //
    ui32Status = am_hal_crm_clock_enable(AM_HAL_CRM_ID_I3C0REF + ui32Module, true, 0, I3C_CLOCK_DIV_VALUE);
    if ( ui32Status != AM_HAL_STATUS_SUCCESS )
    {
        return AM_DEVICES_ICM42688P_STATUS_ERROR;
    }
#endif

    pDevHandle->ui8StaticAddr  = AM_DEVICES_ICM42688P_STATIC_ADDRESS1;
    pDeviceCfg->eTransferMode  = pDevConfig->eTransferMode;
    //
    // Force SDR0 for DASA / CCC / device_init. Enter HDR-DDR after setup if requested.
    //
    pDeviceCfg->eSpeedMode     = AM_HAL_I3C_SDR0;
    pDeviceCfg->ui8DynamicAddr = pDevConfig->ui8DynamicAddr;

    gAmI3cIcm42688p[ui32Index].bOccupied = true;
    gAmI3cIcm42688p[ui32Index].ui32Module = ui32Module;
    gAmI3cIcm42688p[ui32Index].pI3cHandle = pI3cHandle;
    *ppDevHandle = pDevHandle;

    if (pDevConfig->eTransferMode == AM_HAL_I3C_XFER_DMA)
    {
        if (pDevConfig->sI3cDmaCfg.pDMAComplete == NULL)
        {
            ui32Status = AM_DEVICES_ICM42688P_STATUS_INVALID_ARG;
            goto fail_clear_occupied;
        }

        if (pDevConfig->sI3cDmaCfg.pfnI3cTimeoutCallback == NULL)
        {
            pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback = am_devices_i3c_icm42688p_timeout_callback;
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
    ui32Status = am_devices_i3c_icm42688p_send_cccs(&gAmI3cIcm42688p[ui32Index], &ui32CmdData, AM_HAL_I3C_CCC_RSTDAA(true));
    if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Failed to reset ICM-42688-P dynamic address (RSTDAA)!\n");
        goto fail_clear_occupied;
    }

    //
    // Send SETDASA ccc to set the dynamic address of sensor
    //
    ui32Status = am_devices_i3c_icm42688p_send_cccs(&gAmI3cIcm42688p[ui32Index], &ui32CmdData, AM_HAL_I3C_CCC_SETDASA);
    if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Failed to set ICM-42688-P dynamic address (SETDASA)!\n");
        goto fail_clear_occupied;
    }

    //
    // Read device ID
    //
    ui32Status = am_devices_i3c_icm42688p_read_id(&gAmI3cIcm42688p[ui32Index], &ui32DeviceId);
    if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Failed to read device id!\n");
        goto fail_clear_occupied;
    }

    if ((ui32DeviceId & 0xFF) == AM_DEVICES_ICM42688P_DEVICE_ID)
    {
        am_util_stdio_printf("Get ICM-42688-P device id:0x%x!\n", ui32DeviceId);
    }
    else
    {
        am_util_stdio_printf("Get ICM-42688-P device id failed!\n");
        ui32Status = AM_DEVICES_ICM42688P_STATUS_ERROR;
        goto fail_clear_occupied;
    }

    //
    // Set ICM-42688-P max write/read length
    //
    uint32_t ui32MaxWrLen = AM_DEVICES_ICM42688P_MAX_WRITE_LENGTH;
    uint32_t ui32MaxRdLen = AM_DEVICES_ICM42688P_MAX_READ_LENGTH;

    ui32Status = am_devices_i3c_icm42688p_send_cccs(&gAmI3cIcm42688p[ui32Index], &ui32MaxWrLen, AM_HAL_I3C_CCC_SETMWL(false));
    if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Failed to set ICM-42688-P max write length!\n");
        goto fail_clear_occupied;
    }

    ui32Status = am_devices_i3c_icm42688p_send_cccs(&gAmI3cIcm42688p[ui32Index], &ui32MaxRdLen, AM_HAL_I3C_CCC_SETMRL(false));
    if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Failed to set ICM-42688-P max read length!\n");
        goto fail_clear_occupied;
    }

    ui32Status = am_devices_i3c_icm42688p_device_init(&gAmI3cIcm42688p[ui32Index]);
    if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Failed to init ICM-42688-P!\n");
        goto fail_clear_occupied;
    }

    if (pDevConfig->eSpeedMode == AM_HAL_I3C_HDR_DDR)
    {
        ui32Status = am_devices_i3c_icm42688p_enter_hdr_mode(&gAmI3cIcm42688p[ui32Index]);
        if (AM_DEVICES_ICM42688P_STATUS_SUCCESS != ui32Status)
        {
            am_util_stdio_printf("Failed to enter ICM-42688-P HDR-DDR mode!\n");
            goto fail_clear_occupied;
        }
    }
    else
    {
        pDeviceCfg->eSpeedMode = pDevConfig->eSpeedMode;
    }

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;

fail_clear_occupied:
    gAmI3cIcm42688p[ui32Index].bOccupied = false;
    *ppDevHandle = NULL;
    return ui32Status;
}

//*****************************************************************************
//
//! @brief De-Initialize the ICM-42688-P driver.
//!
//! @param pHandle     - Pointer to device handle
//!
//! Releases the device handle. If HDR-DDR is active, exits HDR-DDR first.
//!
//! @return Status.
//
//*****************************************************************************
uint32_t
am_devices_i3c_icm42688p_term(void *pHandle)
{
    uint32_t ui32Status;
    am_devices_i3c_icm42688p_t *pDevHandle = (am_devices_i3c_icm42688p_t *)pHandle;
    am_devices_i3c_icm42688p_config_t *pDeviceCfg;

    if (!pHandle)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE;
    }

    pDeviceCfg = &pDevHandle->sI3cIcm42688pCfg;

    if (pDevHandle->ui32Module >= AM_REG_I3C_NUM_MODULES)
    {
        return AM_DEVICES_ICM42688P_STATUS_OUT_OF_RANGE;
    }

    //
    // Exit HDR-DDR before releasing the handle when still in HDR mode.
    //
    if (pDeviceCfg->eSpeedMode == AM_HAL_I3C_HDR_DDR)
    {
        ui32Status = am_devices_i3c_icm42688p_exit_hdr_mode(pHandle);
        if (ui32Status != AM_DEVICES_ICM42688P_STATUS_SUCCESS)
        {
            return ui32Status;
        }
    }

    //
    // Free this device handle
    //
    pDevHandle->bOccupied = false;
    pDeviceCfg->sI3cDmaCfg.pfnI3cTimeoutCallback = NULL;
    pDeviceCfg->sI3cDmaCfg.pDMAComplete = NULL;

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Write generic device register of ICM-42688-P sensor.
//!
//! @param pHandle        - Pointer to device handle
//! @param ui8SubAddress  - Address of desired data in sensor
//! @param pData          - Buffer to write the sensor data from
//! @param ui32NumBytes   - Number of bytes to write to sensor
//!
//! This function writes data to the given register of sensor, and returns
//! the result as a 32-bit unsigned integer value.
//!
//! @note For SDR writes, byte0 of pData is overwritten with ui8SubAddress.
//!       Place the register payload starting at byte1 (for a single-byte
//!       write, pass the value in bits[15:8], e.g. value << 8).
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_i3c_icm42688p_write_reg(void *pHandle, uint8_t ui8SubAddress,
                                   uint32_t *pData, uint32_t ui32NumBytes)
{
    if (!pHandle)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE;
    }

    if (!pData)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_ARG;
    }

    return am_devices_i3c_icm42688p_xfer(pHandle, ui8SubAddress, pData, ui32NumBytes, false);
}

//*****************************************************************************
//
//! @brief Read generic device register from ICM-42688-P sensor.
//!
//! @param pHandle        - Pointer to device handle
//! @param ui8SubAddress  - Address of desired data in sensor
//! @param pData          - Buffer to store the received data from sensor
//! @param ui32NumBytes   - Number of bytes to read from sensor
//!
//! This function reads data from the given register of sensor, and returns
//! the result as a 32-bit unsigned integer value.
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_i3c_icm42688p_read_reg(void *pHandle, uint8_t ui8SubAddress,
                                  uint32_t *pData, uint32_t ui32NumBytes)
{
    if (!pHandle)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE;
    }

    if (!pData)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_ARG;
    }

    return am_devices_i3c_icm42688p_xfer(pHandle, ui8SubAddress, pData, ui32NumBytes, true);
}

//*****************************************************************************
//
//! @brief Get the raw temperature register value from ICM-42688-P sensor.
//!
//! @param pHandle   - Pointer to device handle
//! @param pui16Data - Pointer to store the raw 16-bit temperature register value
//!
//! This function reads the sensor temperature registers and returns the raw
//! combined register value. It does not convert the value to degrees Celsius.
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_i3c_icm42688p_get_temperature(void *pHandle, uint16_t *pui16Data)
{
    uint8_t ui8TempData[2] __attribute__((aligned(4)));

    if (pui16Data == NULL)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_ARG;
    }

    //
    // Get temperature output high and low registers' data of sensor
    //
    if (am_devices_i3c_icm42688p_read_reg(pHandle, AM_DEVICES_ICM42688P_TEMP_DATA, (uint32_t *)ui8TempData, 2))
    {
        return AM_DEVICES_ICM42688P_STATUS_ERROR;
    }

    pui16Data[0] = (ui8TempData[0] * 256) + ui8TempData[1];

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

uint32_t
am_devices_i3c_icm42688p_acceleration_raw_get(void *pHandle, uint16_t *pui16Data)
{
    uint8_t buff[6]  __attribute__((aligned(4)));

    if (pui16Data == NULL)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_ARG;
    }

    if (am_devices_i3c_icm42688p_read_reg(pHandle, AM_DEVICES_ICM42688P_ACCEL_DATA, (uint32_t *)buff, 6))
    {
        return AM_DEVICES_ICM42688P_STATUS_ERROR;
    }


    pui16Data[0] = (buff[0] * 256) + buff[1];
    pui16Data[1] = (buff[2] * 256) + buff[3];
    pui16Data[2] = (buff[4] * 256) + buff[5];

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

uint32_t
am_devices_i3c_icm42688p_angular_rate_raw_get(void *pHandle, uint16_t *pui16Data)
{
    uint8_t buff[6] __attribute__((aligned(4)));

    if (pui16Data == NULL)
    {
        return AM_DEVICES_ICM42688P_STATUS_INVALID_ARG;
    }

    if (am_devices_i3c_icm42688p_read_reg(pHandle, AM_DEVICES_ICM42688P_GYRO_DATA, (uint32_t *)buff, 6))
    {
        return AM_DEVICES_ICM42688P_STATUS_ERROR;
    }

    pui16Data[0] = (buff[0] * 256) + buff[1];
    pui16Data[1] = (buff[2] * 256) + buff[3];
    pui16Data[2] = (buff[4] * 256) + buff[5];

    return AM_DEVICES_ICM42688P_STATUS_SUCCESS;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

