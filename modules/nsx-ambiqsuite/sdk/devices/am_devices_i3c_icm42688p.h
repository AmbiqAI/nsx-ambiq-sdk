//*****************************************************************************
//
//! @file am_devices_i3c_icm42688p.h
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

#ifndef AM_DEVICES_I3C_ICM42688P_H
#define AM_DEVICES_I3C_ICM42688P_H

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(AM_PART_ATOMIQ110)
#define I3C_CLOCK_DIV_ENABLE
#endif

#ifdef I3C_CLOCK_DIV_ENABLE
#include "hal/am_hal_crm_private.h"

#ifdef ATOMIQ11X_FPGA
#define I3C_CLOCK_DIV_VALUE           1
#else
#define I3C_CLOCK_DIV_VALUE           4
#endif
#endif

//*****************************************************************************
//
//! @name Global definitions for the ICM-42688-P sensor
//! @{
//
//*****************************************************************************
#define AM_DEVICES_ICM42688P_MAX_DEVICE_NUM    1

#define AM_DEVICES_ICM42688P_DEVICE_ID         0x47
#define AM_DEVICES_ICM42688P_STATIC_ADDRESS1   0x68
#define AM_DEVICES_ICM42688P_STATIC_ADDRESS2   0x69
#define AM_DEVICES_ICM42688P_DYNAMIC_ADDRESS   0x42
#define AM_DEVICES_ICM42688P_MAX_WRITE_LENGTH  0x40FF
#define AM_DEVICES_ICM42688P_MAX_READ_LENGTH   0x2020FF
#define AM_DEVICES_ICM42688P_DMA_DELAY_US      100

#define AM_DEVICES_ICM42688P_DEVICE_CONFIG     0x11
#define AM_DEVICES_ICM42688P_TEMP_DATA         0x1D
#define AM_DEVICES_ICM42688P_ACCEL_DATA        0x1F
#define AM_DEVICES_ICM42688P_GYRO_DATA         0x25
#define AM_DEVICES_ICM42688P_INT_STATUS        0x2D
#define AM_DEVICES_ICM42688P_PWR_MGMT0         0x4E
#define AM_DEVICES_ICM42688P_GYRO_CONFIG0      0x4F
#define AM_DEVICES_ICM42688P_ACCEL_CONFIG0     0x50
#define AM_DEVICES_ICM42688P_WHO_AM_I          0x75
#define AM_DEVICES_ICM42688P_BANK_SEL          0x76
#define AM_DEVICES_ICM42688P_INTF_CONFIG6      0x7C
//
// Bank4 register used by HDR-DDR enter sequence (see i3c_hdr_ddr_test_cases).
//
#define AM_DEVICES_ICM42688P_HDR_CFG_REG       0x52


#define AM_DEVICES_ICM42688P_RESET_ENABLE        1U
#define AM_DEVICES_ICM42688P_SOFTWARE_RESET_MSK 0x10U
#define AM_DEVICES_ICM42688P_I3C_ENABLE_MSK    (1 << 4)
#define AM_DEVICES_ICM42688P_DDR_ENABLE_MSK    (1 << 1)
#define AM_DEVICES_ICM42688P_SDR_ENABLE_MSK    (1 << 0)

#define AM_DEVICES_ICM42688P_ACCEL_ENABLE_MSK  (3 << 2)
#define AM_DEVICES_ICM42688P_GYRO_ENABLE_MSK   (3 << 0)
#define AM_DEVICES_ICM42688P_GYRO_FS_SEL_250   (3 << 5)
#define AM_DEVICES_ICM42688P_GYRO_ODR_50HZ     (9 << 0)
#define AM_DEVICES_ICM42688P_ACCEL_FS_SEL_2G   (3 << 5)
#define AM_DEVICES_ICM42688P_ACCEL_ODR_50HZ    (9 << 0)

#define AM_DEVICES_ICM42688P_CMD_RING_LENGTH     256
#define AM_DEVICES_ICM42688P_RESP_RING_LENGTH    256
#define AM_DEVICES_ICM42688P_DMA_TIMEOUT_CNT     1000
#define AM_DEVICES_ICM42688P_RESET_TIMEOUT_CNT   100
#define AM_DEVICES_ICM42688P_DCACHE_SIZE      ( 32*1024 )
//! @}

//*****************************************************************************
//
// Global type definitions.
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_ICM42688P_STATUS_SUCCESS,
    AM_DEVICES_ICM42688P_STATUS_ERROR,
    AM_DEVICES_ICM42688P_STATUS_INVALID_HANDLE,
    AM_DEVICES_ICM42688P_STATUS_IN_USE,
    AM_DEVICES_ICM42688P_STATUS_TIMEOUT,
    AM_DEVICES_ICM42688P_STATUS_OUT_OF_RANGE,
    AM_DEVICES_ICM42688P_STATUS_INVALID_ARG,
} am_devices_i3c_icm42688p_status_t;

typedef struct
{
    uint32_t *pCmdRingBuf;
    uint32_t ui32CmdRingBufLen;
    uint32_t *pRespRingBuf;
    uint32_t ui32RespRingBufLen;

    uint32_t (*pfnI3cTimeoutCallback)(bool *pDMAComplete);
    bool *pDMAComplete;
} am_devices_i3c_icm42688p_dma_config_t;
typedef struct
{
    // Host config
    am_hal_i3c_xfer_mode_e   eTransferMode;
    am_hal_i3c_speed_mode_e  eSpeedMode;

    uint8_t  ui8DynamicAddr;
    am_devices_i3c_icm42688p_dma_config_t sI3cDmaCfg;
} am_devices_i3c_icm42688p_config_t;

typedef enum
{
    AM_DEVICES_ICM42688P_BANK0 = 0,
    AM_DEVICES_ICM42688P_BANK1 = 1,
    AM_DEVICES_ICM42688P_BANK2 = 2,
    AM_DEVICES_ICM42688P_BANK3 = 3,
    AM_DEVICES_ICM42688P_BANK4 = 4,
} am_devices_i3c_icm42688p_bank_e;

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Initialize the ICM-42688-P sensor.
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
extern uint32_t am_devices_i3c_icm42688p_init(uint32_t ui32Module,
                                              void *pI3cHandle,
                                              void **ppDevHandle,
                                              am_devices_i3c_icm42688p_config_t *pDevConfig);

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
extern uint32_t am_devices_i3c_icm42688p_term(void *pHandle);

//*****************************************************************************
//
//! @brief Enter ICM-42688-P HDR-DDR mode.
//!
//! @param pHandle - Pointer to device handle
//!
//! Matches i3c_hdr_ddr_test_cases enter sequence:
//!   1) Select bank4
//!   2) RMW reg 0x52, clear bit4 (I3C_ENABLE_MSK)
//!   3) Restore bank0 for normal register access
//!   4) Set host-side transfer speed to AM_HAL_I3C_HDR_DDR
//!
//! Register programming is done in SDR0; subsequent transfers use HDR-DDR.
//!
//! @return Status.
//
//*****************************************************************************
extern uint32_t am_devices_i3c_icm42688p_enter_hdr_mode(void *pHandle);

//*****************************************************************************
//
//! @brief Exit ICM-42688-P HDR-DDR mode (reverse of enter_hdr_mode).
//!
//! @param pHandle - Pointer to device handle
//!
//! Sequence:
//!   1) Switch host-side speed to AM_HAL_I3C_SDR0
//!   2) Select bank4
//!   3) RMW reg 0x52, set bit4 (I3C_ENABLE_MSK)
//!   4) Restore bank0 for normal register access
//!
//! @return Status.
//
//*****************************************************************************
extern uint32_t am_devices_i3c_icm42688p_exit_hdr_mode(void *pHandle);

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
extern uint32_t am_devices_i3c_icm42688p_send_cccs(void *pHandle,
                                                   uint32_t *pui32Data,
                                                   uint8_t ui8CccId);


//*****************************************************************************
//
//! @brief Write generic device register of sensor.
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
extern uint32_t am_devices_i3c_icm42688p_write_reg(void *pHandle,
                                                   uint8_t ui8SubAddress,
                                                   uint32_t *pData,
                                                   uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Read generic device register from sensor.
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
extern uint32_t am_devices_i3c_icm42688p_read_reg(void *pHandle,
                                                  uint8_t ui8SubAddress,
                                                  uint32_t *pData,
                                                  uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Get the raw temperature register value from sensor.
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
extern uint32_t am_devices_i3c_icm42688p_get_temperature(void *pHandle,
                                                         uint16_t *pui16Data);
//*****************************************************************************
//
//! @brief Get the acceleration data from sensor.
//!
//! @param pHandle        - Pointer to device handle
//! @param pui16Data      - Pointer to the 6 byte accelerometer data
//!
//! This function reads data from the sensor registers
//!
//! @return 32-bit status
//
//*****************************************************************************
extern uint32_t am_devices_i3c_icm42688p_acceleration_raw_get(void *pHandle,
                                                              uint16_t *pui16Data);

//*****************************************************************************
//
//! @brief Get the angular data from sensor.
//!
//! @param pHandle        - Pointer to device handle
//! @param pui16Data      - Pointer to the 6 byte gyroscope data
//!
//! This function reads data from the sensor registers
//!
//! @return 32-bit status
//
//*****************************************************************************
extern uint32_t am_devices_i3c_icm42688p_angular_rate_raw_get(void *pHandle,
                                                              uint16_t *pui16Data);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_I3C_ICM42688P_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

