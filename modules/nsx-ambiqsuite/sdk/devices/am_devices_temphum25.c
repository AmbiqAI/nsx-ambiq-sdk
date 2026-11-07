//*****************************************************************************
//
//! @file am_devices_temphum25.c
//!
//! @brief Device driver for the MikroElektronika TempHum 25 Click board.
//!
//! @addtogroup devices_temphum25 TempHum 25 Click Device Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides an Ambiq HAL device driver for the TempHum 25
//!          Click board. The Click uses an I2C command protocol with CRC8-
//!          protected measurement frames. The driver expects the application to
//!          configure and enable the IOM/I2C interface and optional reset and
//!          alert GPIOs before calling am_devices_temphum25_init().
//!
//! @section devices_temphum25_usage Usage
//!
//! 1. Initialize and enable an IOM in I2C mode.
//! 2. Configure mikroBUS SCL/SDA (and optional RST/INT) pins in the BSP.
//! 3. Fill an am_devices_temphum25_config_t structure and call init().
//! 4. Use measure/read APIs to obtain temperature and humidity.
//!
//! @section devices_temphum25_configuration Configuration
//!
//! - @b pIomHandle: Initialized Ambiq IOM handle for the Click I2C bus.
//! - @b ui8I2cAddr: Device I2C address, typically 0x44 or 0x45.
//! - @b ui32ResetGpio: Optional reset pin number, or
//!   AM_DEVICES_TEMPHUM25_GPIO_UNCONFIGURED.
//! - @b ui32AlertGpio: Optional alert pin number, or
//!   AM_DEVICES_TEMPHUM25_GPIO_UNCONFIGURED.
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

#include <stdint.h>
#include <stdbool.h>
#include "am_mcu_apollo.h"
#include "am_devices_temphum25.h"
#include "am_util_delay.h"

//*****************************************************************************
//
// Local definitions.
//
//*****************************************************************************
#define TEMPHUM25_CRC_POLYNOM                     0x31
#define TEMPHUM25_CRC_INIT_VALUE                  0xFF

#define TEMPHUM25_MEAS_DELAY_MS                   15
#define TEMPHUM25_CMD_DELAY_MS                    1
#define TEMPHUM25_SOFT_RESET_DELAY_MS             100
#define TEMPHUM25_HW_RESET_DELAY_MS               100

//*****************************************************************************
//
// Device state.
//
//*****************************************************************************
typedef struct
{
    void     *pIomHandle;
    uint8_t   ui8I2cAddr;
    uint16_t  ui16LastCommand;
    uint32_t  ui32ResetGpio;
    uint32_t  ui32AlertGpio;
    bool      bOccupied;
} am_devices_temphum25_t;

static am_devices_temphum25_t gAmTemphum25[AM_DEVICES_TEMPHUM25_MAX_DEVICE_NUM];
static am_devices_temphum25_last_error_t g_sTemphum25LastError;

//*****************************************************************************
//
// Word-aligned I/O staging buffer for I2C command/data transfers.
//
// temphum25_i2c_write()/_read() hand the buffer pointer to the IOM HAL as a
// uint32_t *, and the HAL may touch a full 32-bit word for the final partial
// word of a transfer shorter than 4 bytes (e.g. a 2-byte command write).
// Backing these buffers with this union guarantees 32-bit alignment and
// reserves a full word of storage past any 2/3/6-byte payload, so that
// access stays within the buffer. The I2C transfer length passed to the
// HAL is unaffected and remains the exact protocol length (2, 3, or 6).
//
//*****************************************************************************
typedef union
{
    uint32_t ui32Word[2];
    uint8_t  ui8Byte[8];
} temphum25_iobuf_t;

//*****************************************************************************
//
// Forward declarations.
//
//*****************************************************************************
static void temphum25_record_error(am_devices_temphum25_fail_step_t eStep,
                                   uint32_t ui32DriverStatus,
                                   uint32_t ui32HalStatus,
                                   uint16_t ui16Command,
                                   uint8_t ui8I2cAddr,
                                   uint8_t *pui8RawData,
                                   uint8_t *pui8CrcCalc,
                                   uint8_t *pui8CrcRx);
static uint32_t temphum25_i2c_write(void *pIomHandle, uint8_t ui8DevAddr,
                                    uint8_t *pui8Data, uint32_t ui32NumBytes,
                                    uint32_t *pui32HalStatus);
static uint32_t temphum25_i2c_read(void *pIomHandle, uint8_t ui8DevAddr,
                                   uint8_t *pui8Data, uint32_t ui32NumBytes,
                                   uint32_t *pui32HalStatus);
static uint8_t temphum25_calculate_crc(uint8_t *pui8Source);
static uint32_t temphum25_validate_handle(void *pHandle,
                                          am_devices_temphum25_t **ppsDev);
static uint32_t temphum25_read_raw_data(am_devices_temphum25_t *psDev,
                                        uint8_t *pui8DataBuf);
static uint32_t temphum25_decode_measurement(uint8_t *pui8DataBuf,
                                             float *pfTemperature,
                                             float *pfHumidity);

//*****************************************************************************
//
//! @brief Initialize a TempHum 25 configuration structure with defaults.
//
//*****************************************************************************
void
am_devices_temphum25_config_init(am_devices_temphum25_config_t *psConfig)
{
    if ( psConfig == NULL )
    {
        return;
    }

    //
    // Apply default I2C address and unconfigured GPIO values.
    //
    psConfig->pIomHandle    = NULL;
    psConfig->ui8I2cAddr  = AM_DEVICES_TEMPHUM25_I2C_ADDR_0;
    psConfig->ui32ResetGpio = AM_DEVICES_TEMPHUM25_GPIO_UNCONFIGURED;
    psConfig->ui32AlertGpio = AM_DEVICES_TEMPHUM25_GPIO_UNCONFIGURED;
}

//*****************************************************************************
//
//! @brief Initialize the TempHum 25 driver.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_init(am_devices_temphum25_config_t *psConfig, void **ppHandle)
{
    uint32_t ui32Index;

    if ( (psConfig == NULL) || (ppHandle == NULL) || (psConfig->pIomHandle == NULL) )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG;
    }

    //
    // Find an unused device slot in the driver table.
    //
    for ( ui32Index = 0; ui32Index < AM_DEVICES_TEMPHUM25_MAX_DEVICE_NUM; ui32Index++ )
    {
        if ( gAmTemphum25[ui32Index].bOccupied == false )
        {
            break;
        }
    }

    if ( ui32Index == AM_DEVICES_TEMPHUM25_MAX_DEVICE_NUM )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_IN_USE;
    }

    //
    // Copy config into the slot and return the device handle.
    //
    gAmTemphum25[ui32Index].pIomHandle    = psConfig->pIomHandle;
    gAmTemphum25[ui32Index].ui8I2cAddr    = psConfig->ui8I2cAddr;
    gAmTemphum25[ui32Index].ui16LastCommand = 0;
    gAmTemphum25[ui32Index].ui32ResetGpio = psConfig->ui32ResetGpio;
    gAmTemphum25[ui32Index].ui32AlertGpio = psConfig->ui32AlertGpio;
    gAmTemphum25[ui32Index].bOccupied     = true;

    *ppHandle = &gAmTemphum25[ui32Index];

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Release a TempHum 25 device handle.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_term(void **ppHandle)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Status;

    if ( (ppHandle == NULL) || (*ppHandle == NULL) )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_HANDLE;
    }

    ui32Status = temphum25_validate_handle(*ppHandle, &psDev);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Release the slot and clear the caller's handle pointer.
    //
    psDev->bOccupied = false;
    *ppHandle = NULL;

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Send a 16-bit command word to the sensor.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_send_command(void *pHandle, uint16_t ui16Cmd)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Status;
    uint32_t ui32HalStatus;
    temphum25_iobuf_t uCmdBuf;

    ui32Status = temphum25_validate_handle(pHandle, &psDev);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Pack the 16-bit command into big-endian I2C bytes.
    //
    uCmdBuf.ui8Byte[0] = (uint8_t)((ui16Cmd >> 8) & 0xFF);
    uCmdBuf.ui8Byte[1] = (uint8_t)(ui16Cmd & 0xFF);

    //
    // Write the command word to the sensor over I2C.
    //
    ui32Status = temphum25_i2c_write(psDev->pIomHandle, psDev->ui8I2cAddr,
                                     uCmdBuf.ui8Byte, 2,
                                     &ui32HalStatus);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        temphum25_record_error(AM_DEVICES_TEMPHUM25_FAIL_STEP_CMD_WRITE,
                               ui32Status,
                               ui32HalStatus,
                               ui16Cmd,
                               psDev->ui8I2cAddr,
                               NULL, NULL, NULL);
    }
    else
    {
        //
        // Save the command for later read/error reporting.
        //
        psDev->ui16LastCommand = ui16Cmd;
    }

    return ui32Status;
}

//*****************************************************************************
//
//! @brief Start a measurement using the selected command word.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_start_measurement(void *pHandle, uint16_t ui16MeasCmd)
{
    uint32_t ui32Status;

    ui32Status = am_devices_temphum25_send_command(pHandle, ui16MeasCmd);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Wait for the single-shot conversion to complete.
    //
    am_util_delay_ms(TEMPHUM25_MEAS_DELAY_MS);

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Stop continuous measurement mode.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_stop_measurement(void *pHandle)
{
    uint32_t ui32Status;

    ui32Status = am_devices_temphum25_send_command(pHandle,
                                                    AM_DEVICES_TEMPHUM25_CMD_CONT_MEAS_STOP);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Allow the sensor time to leave periodic mode.
    //
    am_util_delay_ms(TEMPHUM25_CMD_DELAY_MS);

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Read and decode a single-shot measurement result.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_read_measurement(void *pHandle,
                                      float *pfTemperature,
                                      float *pfHumidity)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Status;
    temphum25_iobuf_t uDataBuf;

    ui32Status = temphum25_validate_handle(pHandle, &psDev);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    if ( (pfTemperature == NULL) && (pfHumidity == NULL) )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG;
    }

    //
    // Read the six-byte measurement frame and verify CRC.
    //
    ui32Status = temphum25_read_raw_data(psDev, uDataBuf.ui8Byte);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Convert raw values to degrees C and percent RH.
    //
    return temphum25_decode_measurement(uDataBuf.ui8Byte, pfTemperature, pfHumidity);
}

//*****************************************************************************
//
//! @brief Fetch and decode the latest periodic-mode measurement.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_fetch_measurement(void *pHandle,
                                       float *pfTemperature,
                                       float *pfHumidity)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Status;
    temphum25_iobuf_t uDataBuf;

    ui32Status = temphum25_validate_handle(pHandle, &psDev);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    if ( (pfTemperature == NULL) && (pfHumidity == NULL) )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG;
    }

    //
    // Tell the sensor to prepare the latest periodic sample.
    //
    ui32Status = am_devices_temphum25_send_command(pHandle,
                                                    AM_DEVICES_TEMPHUM25_CMD_FETCH_DATA);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Read and decode the fetched measurement frame.
    //
    ui32Status = temphum25_read_raw_data(psDev, uDataBuf.ui8Byte);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    return temphum25_decode_measurement(uDataBuf.ui8Byte, pfTemperature, pfHumidity);
}

//*****************************************************************************
//
//! @brief Perform a single-shot measurement using I2C clock stretching.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_measure_stretch(void *pHandle,
                                     uint16_t ui16MeasCmd,
                                     float *pfTemperature,
                                     float *pfHumidity)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Status;
    temphum25_iobuf_t uDataBuf;

    ui32Status = temphum25_validate_handle(pHandle, &psDev);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    if ( (pfTemperature == NULL) && (pfHumidity == NULL) )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG;
    }

    //
    // Send a clock-stretch-enabled measure command (_CS_EN).
    //
    ui32Status = am_devices_temphum25_send_command(pHandle, ui16MeasCmd);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Wait the minimum time before starting the read phase.
    //
    am_util_delay_ms(TEMPHUM25_CMD_DELAY_MS);

    //
    // Read while the sensor may hold SCL low until conversion completes.
    //
    ui32Status = temphum25_read_raw_data(psDev, uDataBuf.ui8Byte);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    return temphum25_decode_measurement(uDataBuf.ui8Byte, pfTemperature, pfHumidity);
}

//*****************************************************************************
//
//! @brief Perform a single-shot measurement and return the result.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_measure(void *pHandle,
                             uint16_t ui16MeasCmd,
                             float *pfTemperature,
                             float *pfHumidity)
{
    uint32_t ui32Status;

    //
    // Start the measurement and wait for conversion time.
    //
    ui32Status = am_devices_temphum25_start_measurement(pHandle, ui16MeasCmd);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Read and return the completed measurement.
    //
    return am_devices_temphum25_read_measurement(pHandle, pfTemperature, pfHumidity);
}

//*****************************************************************************
//
//! @brief Read the 32-bit device serial number.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_read_serial_num(void *pHandle, uint32_t *pui32SerialNum)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Status;
    uint32_t ui32HalStatus;
    temphum25_iobuf_t uDataBuf;
    uint8_t ui8CrcCalc[2];
    uint8_t ui8CrcRx[2];

    ui32Status = temphum25_validate_handle(pHandle, &psDev);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    if ( pui32SerialNum == NULL )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG;
    }

    //
    // Request the first half of the serial number.
    //
    ui32Status = am_devices_temphum25_send_command(pHandle,
                                                    AM_DEVICES_TEMPHUM25_CMD_GET_SERIAL_NUM_0);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    am_util_delay_ms(TEMPHUM25_CMD_DELAY_MS);

    //
    // Read six bytes: two ID words plus CRC bytes.
    //
    ui32Status = temphum25_i2c_read(psDev->pIomHandle, psDev->ui8I2cAddr,
                                    uDataBuf.ui8Byte, 6,
                                    &ui32HalStatus);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        temphum25_record_error(AM_DEVICES_TEMPHUM25_FAIL_STEP_DATA_READ,
                               ui32Status,
                               ui32HalStatus,
                               AM_DEVICES_TEMPHUM25_CMD_GET_SERIAL_NUM_0,
                               psDev->ui8I2cAddr,
                               NULL, NULL, NULL);
        return ui32Status;
    }

    //
    // Verify CRC on both 16-bit serial fragments.
    //
    ui8CrcCalc[0] = temphum25_calculate_crc(&uDataBuf.ui8Byte[0]);
    ui8CrcCalc[1] = temphum25_calculate_crc(&uDataBuf.ui8Byte[3]);
    ui8CrcRx[0] = uDataBuf.ui8Byte[2];
    ui8CrcRx[1] = uDataBuf.ui8Byte[5];

    if ( (ui8CrcRx[0] != ui8CrcCalc[0]) ||
         (ui8CrcRx[1] != ui8CrcCalc[1]) )
    {
        temphum25_record_error(AM_DEVICES_TEMPHUM25_FAIL_STEP_CRC_CHECK,
                               AM_DEVICES_TEMPHUM25_STATUS_CRC_ERROR,
                               AM_HAL_STATUS_SUCCESS,
                               AM_DEVICES_TEMPHUM25_CMD_GET_SERIAL_NUM_0,
                               psDev->ui8I2cAddr,
                               uDataBuf.ui8Byte, ui8CrcCalc, ui8CrcRx);
        return AM_DEVICES_TEMPHUM25_STATUS_CRC_ERROR;
    }

    //
    // Combine the fragments into a 32-bit serial number.
    //
    *pui32SerialNum = ((uint32_t)uDataBuf.ui8Byte[0] << 24) |
                      ((uint32_t)uDataBuf.ui8Byte[1] << 16) |
                      ((uint32_t)uDataBuf.ui8Byte[3] << 8) |
                      uDataBuf.ui8Byte[4];

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Read the 16-bit status register.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_read_status(void *pHandle, uint16_t *pui16Status)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Status;
    uint32_t ui32HalStatus;
    temphum25_iobuf_t uDataBuf;
    uint8_t ui8CrcCalc[2] = {0, 0};
    uint8_t ui8CrcRx[2] = {0, 0};

    ui32Status = temphum25_validate_handle(pHandle, &psDev);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    if ( pui16Status == NULL )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG;
    }

    //
    // Send the read-status command to the sensor.
    //
    ui32Status = am_devices_temphum25_send_command(pHandle,
                                                    AM_DEVICES_TEMPHUM25_CMD_READ_STATUS);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Read status plus CRC and validate the frame.
    //
    ui32Status = temphum25_i2c_read(psDev->pIomHandle, psDev->ui8I2cAddr,
                                    uDataBuf.ui8Byte, 3,
                                    &ui32HalStatus);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        temphum25_record_error(AM_DEVICES_TEMPHUM25_FAIL_STEP_DATA_READ,
                               ui32Status,
                               ui32HalStatus,
                               AM_DEVICES_TEMPHUM25_CMD_READ_STATUS,
                               psDev->ui8I2cAddr,
                               NULL, NULL, NULL);
        return ui32Status;
    }

    ui8CrcCalc[0] = temphum25_calculate_crc(&uDataBuf.ui8Byte[0]);
    ui8CrcRx[0] = uDataBuf.ui8Byte[2];

    if ( ui8CrcRx[0] != ui8CrcCalc[0] )
    {
        temphum25_record_error(AM_DEVICES_TEMPHUM25_FAIL_STEP_CRC_CHECK,
                               AM_DEVICES_TEMPHUM25_STATUS_CRC_ERROR,
                               AM_HAL_STATUS_SUCCESS,
                               AM_DEVICES_TEMPHUM25_CMD_READ_STATUS,
                               psDev->ui8I2cAddr,
                               NULL, ui8CrcCalc, ui8CrcRx);
        return AM_DEVICES_TEMPHUM25_STATUS_CRC_ERROR;
    }

    //
    // Return the 16-bit status register value.
    //
    *pui16Status = ((uint16_t)uDataBuf.ui8Byte[0] << 8) | uDataBuf.ui8Byte[1];

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Clear the status register.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_clear_status(void *pHandle)
{
    uint32_t ui32Status;

    //
    // Send the clear-status command.
    //
    ui32Status = am_devices_temphum25_send_command(pHandle,
                                                    AM_DEVICES_TEMPHUM25_CMD_CLEAR_STATUS);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Allow the sensor time to process the command.
    //
    am_util_delay_ms(TEMPHUM25_CMD_DELAY_MS);

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Enable the on-chip heater.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_heater_enable(void *pHandle)
{
    uint32_t ui32Status;

    //
    // Send the heater-enable command.
    //
    ui32Status = am_devices_temphum25_send_command(pHandle,
                                                    AM_DEVICES_TEMPHUM25_CMD_HEATER_ENABLE);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    am_util_delay_ms(TEMPHUM25_CMD_DELAY_MS);

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Disable the on-chip heater.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_heater_disable(void *pHandle)
{
    uint32_t ui32Status;

    //
    // Send the heater-disable command.
    //
    ui32Status = am_devices_temphum25_send_command(pHandle,
                                                    AM_DEVICES_TEMPHUM25_CMD_HEATER_DISABLE);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    am_util_delay_ms(TEMPHUM25_CMD_DELAY_MS);

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Perform a software reset of the sensor.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_soft_reset(void *pHandle)
{
    uint32_t ui32Status;

    //
    // Send the soft-reset command.
    //
    ui32Status = am_devices_temphum25_send_command(pHandle,
                                                    AM_DEVICES_TEMPHUM25_CMD_SOFT_RESET);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Wait for the sensor to reboot after reset.
    //
    am_util_delay_ms(TEMPHUM25_SOFT_RESET_DELAY_MS);

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set the reset GPIO state, if configured.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_reset_pin_set(void *pHandle, bool bState)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Status;

    ui32Status = temphum25_validate_handle(pHandle, &psDev);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    if ( psDev->ui32ResetGpio == AM_DEVICES_TEMPHUM25_GPIO_UNCONFIGURED )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_ERROR;
    }

    //
    // Drive reset high (deasserted) or low (asserted).
    //
    if ( bState )
    {
        am_hal_gpio_output_set(psDev->ui32ResetGpio);
    }
    else
    {
        am_hal_gpio_output_clear(psDev->ui32ResetGpio);
    }

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Toggle the reset GPIO to reset the sensor, if configured.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_reset_device(void *pHandle)
{
    uint32_t ui32Status;

    //
    // Assert reset, wait, then deassert reset.
    //
    ui32Status = am_devices_temphum25_reset_pin_set(pHandle, false);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    am_util_delay_ms(TEMPHUM25_HW_RESET_DELAY_MS);

    ui32Status = am_devices_temphum25_reset_pin_set(pHandle, true);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    //
    // Allow the sensor to start up after hardware reset.
    //
    am_util_delay_ms(TEMPHUM25_HW_RESET_DELAY_MS);

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Read the alert GPIO state, if configured.
//
//*****************************************************************************
uint32_t
am_devices_temphum25_alert_get(void *pHandle, bool *pbAlert)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Status;

    ui32Status = temphum25_validate_handle(pHandle, &psDev);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        return ui32Status;
    }

    if ( pbAlert == NULL )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG;
    }

    if ( psDev->ui32AlertGpio == AM_DEVICES_TEMPHUM25_GPIO_UNCONFIGURED )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_ERROR;
    }

    //
    // Read the alert pin level from GPIO.
    //
    *pbAlert = am_hal_gpio_input_read(psDev->ui32AlertGpio);

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Clear the stored driver last-error record.
//
//*****************************************************************************
void
am_devices_temphum25_clear_last_error(void)
{
    uint32_t ui32Index;

    g_sTemphum25LastError.eStep              = AM_DEVICES_TEMPHUM25_FAIL_STEP_NONE;
    g_sTemphum25LastError.ui32DriverStatus    = AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
    g_sTemphum25LastError.ui32HalStatus       = AM_HAL_STATUS_SUCCESS;
    g_sTemphum25LastError.ui16Command         = 0;
    g_sTemphum25LastError.ui8I2cAddr          = 0;
    g_sTemphum25LastError.bRawDataValid       = false;
    for ( ui32Index = 0; ui32Index < 6; ui32Index++ )
    {
        g_sTemphum25LastError.ui8RawData[ui32Index] = 0;
    }
    g_sTemphum25LastError.ui8CrcCalc[0]       = 0;
    g_sTemphum25LastError.ui8CrcCalc[1]       = 0;
    g_sTemphum25LastError.ui8CrcRx[0]         = 0;
    g_sTemphum25LastError.ui8CrcRx[1]         = 0;
}

//*****************************************************************************
//
//! @brief Read the most recent driver error details.
//
//*****************************************************************************
void
am_devices_temphum25_get_last_error(am_devices_temphum25_last_error_t *psError)
{
    if ( psError != NULL )
    {
        //
        // Copy the most recent error record to the caller.
        //
        *psError = g_sTemphum25LastError;
    }
}

//*****************************************************************************
//
//! @brief Return a human-readable driver status name.
//
//*****************************************************************************
const char *
am_devices_temphum25_status_name(uint32_t ui32Status)
{
    switch ( ui32Status )
    {
        case AM_DEVICES_TEMPHUM25_STATUS_SUCCESS:
            return "SUCCESS";
        case AM_DEVICES_TEMPHUM25_STATUS_ERROR:
            return "ERROR";
        case AM_DEVICES_TEMPHUM25_STATUS_INVALID_HANDLE:
            return "INVALID_HANDLE";
        case AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG:
            return "INVALID_ARG";
        case AM_DEVICES_TEMPHUM25_STATUS_CRC_ERROR:
            return "CRC_ERROR";
        case AM_DEVICES_TEMPHUM25_STATUS_IN_USE:
            return "IN_USE";
        default:
            return "UNKNOWN";
    }
}

//*****************************************************************************
//
//! @brief Return a human-readable failure-step name.
//
//*****************************************************************************
const char *
am_devices_temphum25_fail_step_name(am_devices_temphum25_fail_step_t eStep)
{
    switch ( eStep )
    {
        case AM_DEVICES_TEMPHUM25_FAIL_STEP_CMD_WRITE:
            return "CMD_WRITE";
        case AM_DEVICES_TEMPHUM25_FAIL_STEP_DATA_READ:
            return "DATA_READ";
        case AM_DEVICES_TEMPHUM25_FAIL_STEP_CRC_CHECK:
            return "CRC_CHECK";
        case AM_DEVICES_TEMPHUM25_FAIL_STEP_NONE:
        default:
            return "NONE";
    }
}

//*****************************************************************************
//
//! @brief Read the six-byte measurement frame from the sensor.
//
//*****************************************************************************
static uint32_t
temphum25_read_raw_data(am_devices_temphum25_t *psDev, uint8_t *pui8DataBuf)
{
    uint32_t ui32Status;
    uint32_t ui32HalStatus;
    uint8_t ui8CrcCalc[2];
    uint8_t ui8CrcRx[2];

    //
    // Issue an I2C read for the six-byte measurement frame.
    //
    ui32Status = temphum25_i2c_read(psDev->pIomHandle, psDev->ui8I2cAddr,
                                     pui8DataBuf, 6, &ui32HalStatus);
    if ( ui32Status != AM_DEVICES_TEMPHUM25_STATUS_SUCCESS )
    {
        temphum25_record_error(AM_DEVICES_TEMPHUM25_FAIL_STEP_DATA_READ,
                               ui32Status,
                               ui32HalStatus,
                               psDev->ui16LastCommand,
                               psDev->ui8I2cAddr,
                               NULL,
                               NULL, NULL);
        return ui32Status;
    }

    //
    // Verify CRC on the temperature and humidity data pairs.
    //
    ui8CrcCalc[0] = temphum25_calculate_crc(&pui8DataBuf[0]);
    ui8CrcCalc[1] = temphum25_calculate_crc(&pui8DataBuf[3]);
    ui8CrcRx[0] = pui8DataBuf[2];
    ui8CrcRx[1] = pui8DataBuf[5];

    if ( (ui8CrcRx[0] != ui8CrcCalc[0]) || (ui8CrcRx[1] != ui8CrcCalc[1]) )
    {
        temphum25_record_error(AM_DEVICES_TEMPHUM25_FAIL_STEP_CRC_CHECK,
                               AM_DEVICES_TEMPHUM25_STATUS_CRC_ERROR,
                               AM_HAL_STATUS_SUCCESS,
                               psDev->ui16LastCommand,
                               psDev->ui8I2cAddr,
                               pui8DataBuf,
                               ui8CrcCalc,
                               ui8CrcRx);
        return AM_DEVICES_TEMPHUM25_STATUS_CRC_ERROR;
    }

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Decode a validated six-byte measurement frame.
//
//*****************************************************************************
static uint32_t
temphum25_decode_measurement(uint8_t *pui8DataBuf,
                             float *pfTemperature,
                             float *pfHumidity)
{
    uint16_t ui16RawData;

    if ( pfTemperature != NULL )
    {
        //
        // Scale raw temperature to degrees Celsius.
        //
        ui16RawData = ((uint16_t)pui8DataBuf[0] << 8) | pui8DataBuf[1];
        *pfTemperature = AM_DEVICES_TEMPHUM25_ABS_MIN_TEMP +
            (AM_DEVICES_TEMPHUM25_ABS_MAX_TEMP - AM_DEVICES_TEMPHUM25_ABS_MIN_TEMP) *
            ((float)ui16RawData / AM_DEVICES_TEMPHUM25_DATA_RESOLUTION);
    }

    if ( pfHumidity != NULL )
    {
        //
        // Scale raw humidity to percent relative humidity.
        //
        ui16RawData = ((uint16_t)pui8DataBuf[3] << 8) | pui8DataBuf[4];
        *pfHumidity = AM_DEVICES_TEMPHUM25_MAX_RH *
            ((float)ui16RawData / AM_DEVICES_TEMPHUM25_DATA_RESOLUTION);
    }

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Perform a blocking I2C write transfer.
//
//*****************************************************************************
static uint32_t
temphum25_i2c_write(void *pIomHandle, uint8_t ui8DevAddr,
                    uint8_t *pui8Data, uint32_t ui32NumBytes,
                    uint32_t *pui32HalStatus)
{
    am_hal_iom_transfer_t sTransaction;
    uint32_t ui32HalStatus;

    if ( (pIomHandle == NULL) || ((pui8Data == NULL) && (ui32NumBytes != 0)) )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG;
    }

    //
    // Fill in a blocking I2C write transaction.
    //
    sTransaction.ui32InstrLen    = 0;
#if (defined(AM_PART_APOLLO4B) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || \
     defined(AM_PART_APOLLO5_API) || defined(AM_PART_ATOMIQ110))
    sTransaction.ui64Instr       = 0;
#else
    sTransaction.ui32Instr       = 0;
#endif
    sTransaction.eDirection      = AM_HAL_IOM_TX;
    sTransaction.ui32NumBytes    = ui32NumBytes;
    sTransaction.pui32TxBuffer   = (uint32_t *)pui8Data;
    sTransaction.pui32RxBuffer   = NULL;
    sTransaction.uPeerInfo.ui32I2CDevAddr = ui8DevAddr;
    sTransaction.bContinue       = false;
    sTransaction.ui8RepeatCount  = 0;
    sTransaction.ui32PauseCondition = 0;
    sTransaction.ui32StatusSetClr = 0;

    //
    // Run the transfer and map HAL errors to driver status.
    //
    ui32HalStatus = am_hal_iom_blocking_transfer(pIomHandle, &sTransaction);
    if ( pui32HalStatus != NULL )
    {
        *pui32HalStatus = ui32HalStatus;
    }

    if ( ui32HalStatus != AM_HAL_STATUS_SUCCESS )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_ERROR;
    }

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Perform a blocking I2C read transfer.
//
//*****************************************************************************
static uint32_t
temphum25_i2c_read(void *pIomHandle, uint8_t ui8DevAddr,
                   uint8_t *pui8Data, uint32_t ui32NumBytes,
                   uint32_t *pui32HalStatus)
{
    am_hal_iom_transfer_t sTransaction;
    uint32_t ui32HalStatus;

    if ( (pIomHandle == NULL) || ((pui8Data == NULL) && (ui32NumBytes != 0)) )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG;
    }

    //
    // Fill in a blocking I2C read transaction.
    //
    sTransaction.ui32InstrLen    = 0;
#if (defined(AM_PART_APOLLO4B) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || \
     defined(AM_PART_APOLLO5_API) || defined(AM_PART_ATOMIQ110))
    sTransaction.ui64Instr       = 0;
#else
    sTransaction.ui32Instr       = 0;
#endif
    sTransaction.eDirection      = AM_HAL_IOM_RX;
    sTransaction.ui32NumBytes    = ui32NumBytes;
    sTransaction.pui32TxBuffer   = NULL;
    sTransaction.pui32RxBuffer   = (uint32_t *)pui8Data;
    sTransaction.uPeerInfo.ui32I2CDevAddr = ui8DevAddr;
    sTransaction.bContinue       = false;
    sTransaction.ui8RepeatCount  = 0;
    sTransaction.ui32PauseCondition = 0;
    sTransaction.ui32StatusSetClr = 0;

    //
    // Run the transfer and map HAL errors to driver status.
    //
    ui32HalStatus = am_hal_iom_blocking_transfer(pIomHandle, &sTransaction);
    if ( pui32HalStatus != NULL )
    {
        *pui32HalStatus = ui32HalStatus;
    }

    if ( ui32HalStatus != AM_HAL_STATUS_SUCCESS )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_ERROR;
    }

    return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Store details about the most recent failure.
//
//*****************************************************************************
static void
temphum25_record_error(am_devices_temphum25_fail_step_t eStep,
                       uint32_t ui32DriverStatus,
                       uint32_t ui32HalStatus,
                       uint16_t ui16Command,
                       uint8_t ui8I2cAddr,
                       uint8_t *pui8RawData,
                       uint8_t *pui8CrcCalc,
                       uint8_t *pui8CrcRx)
{
    uint32_t ui32Index;

    //
    // Save error context for the application to inspect later.
    //
    g_sTemphum25LastError.eStep          = eStep;
    g_sTemphum25LastError.ui32DriverStatus = ui32DriverStatus;
    g_sTemphum25LastError.ui32HalStatus  = ui32HalStatus;
    g_sTemphum25LastError.ui16Command    = ui16Command;
    g_sTemphum25LastError.ui8I2cAddr     = ui8I2cAddr;
    g_sTemphum25LastError.bRawDataValid  = false;

    for ( ui32Index = 0; ui32Index < 6; ui32Index++ )
    {
        g_sTemphum25LastError.ui8RawData[ui32Index] = 0;
    }
    g_sTemphum25LastError.ui8CrcCalc[0] = 0;
    g_sTemphum25LastError.ui8CrcCalc[1] = 0;
    g_sTemphum25LastError.ui8CrcRx[0] = 0;
    g_sTemphum25LastError.ui8CrcRx[1] = 0;

    if ( pui8RawData != NULL )
    {
        for ( ui32Index = 0; ui32Index < 6; ui32Index++ )
        {
            g_sTemphum25LastError.ui8RawData[ui32Index] = pui8RawData[ui32Index];
        }
        g_sTemphum25LastError.bRawDataValid = true;
    }

    if ( pui8CrcCalc != NULL )
    {
        g_sTemphum25LastError.ui8CrcCalc[0] = pui8CrcCalc[0];
        g_sTemphum25LastError.ui8CrcCalc[1] = pui8CrcCalc[1];
    }

    if ( pui8CrcRx != NULL )
    {
        g_sTemphum25LastError.ui8CrcRx[0] = pui8CrcRx[0];
        g_sTemphum25LastError.ui8CrcRx[1] = pui8CrcRx[1];
    }
}

//*****************************************************************************
//
//! @brief Calculate the sensor CRC8 for a 2-byte payload.
//
//*****************************************************************************
static uint8_t
temphum25_calculate_crc(uint8_t *pui8Source)
{
    uint8_t ui8Crc = TEMPHUM25_CRC_INIT_VALUE;
    uint8_t ui8ByteCnt;
    uint8_t ui8BitCnt;

    //
    // Run CRC8 over each byte of the 2-byte payload.
    //
    for ( ui8ByteCnt = 0; ui8ByteCnt < 2; ui8ByteCnt++ )
    {
        ui8Crc ^= pui8Source[ui8ByteCnt];

        for ( ui8BitCnt = 0; ui8BitCnt < 8; ui8BitCnt++ )
        {
            if ( ui8Crc & 0x80 )
            {
                ui8Crc = (uint8_t)((ui8Crc << 1) ^ TEMPHUM25_CRC_POLYNOM);
            }
            else
            {
                ui8Crc <<= 1;
            }
        }
    }

    return ui8Crc;
}

//*****************************************************************************
//
//! @brief Validate a device handle returned from init().
//
//*****************************************************************************
static uint32_t
temphum25_validate_handle(void *pHandle, am_devices_temphum25_t **ppsDev)
{
    am_devices_temphum25_t *psDev;
    uint32_t ui32Index;

    if ( (pHandle == NULL) || (ppsDev == NULL) )
    {
        return AM_DEVICES_TEMPHUM25_STATUS_INVALID_HANDLE;
    }

    psDev = (am_devices_temphum25_t *)pHandle;

    //
    // Confirm the handle matches an occupied driver slot.
    //
    for ( ui32Index = 0; ui32Index < AM_DEVICES_TEMPHUM25_MAX_DEVICE_NUM; ui32Index++ )
    {
        if ( (psDev == &gAmTemphum25[ui32Index]) && (gAmTemphum25[ui32Index].bOccupied) )
        {
            *ppsDev = psDev;
            return AM_DEVICES_TEMPHUM25_STATUS_SUCCESS;
        }
    }

    return AM_DEVICES_TEMPHUM25_STATUS_INVALID_HANDLE;
}

//! @}
//
//*****************************************************************************
