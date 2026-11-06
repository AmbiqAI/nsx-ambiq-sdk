//*****************************************************************************
//
//! @file am_devices_temphum25.h
//!
//! @brief Device driver for the MikroElektronika TempHum 25 Click board.
//!
//! @addtogroup devices_temphum25 TempHum 25 Click Device Driver
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

#ifndef AM_DEVICES_TEMPHUM25_H
#define AM_DEVICES_TEMPHUM25_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name Device constants
//! @{
//
//*****************************************************************************
#define AM_DEVICES_TEMPHUM25_MAX_DEVICE_NUM       1

#define AM_DEVICES_TEMPHUM25_I2C_ADDR_0           0x44
#define AM_DEVICES_TEMPHUM25_I2C_ADDR_1           0x45

#define AM_DEVICES_TEMPHUM25_GPIO_UNCONFIGURED    0xFFFFFFFF

#define AM_DEVICES_TEMPHUM25_MAX_RH               100.0f
#define AM_DEVICES_TEMPHUM25_MIN_RH               0.0f
#define AM_DEVICES_TEMPHUM25_DATA_RESOLUTION      65535.0f
#define AM_DEVICES_TEMPHUM25_ABS_MIN_TEMP         (-45.0f)
#define AM_DEVICES_TEMPHUM25_ABS_MAX_TEMP         130.0f
//! @}

//*****************************************************************************
//
//! @name TempHum 25 command words
//! @{
//
//*****************************************************************************
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_SSH_REP_HIGH_CS_EN      0x2C06
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_SSH_REP_MID_CS_EN       0x2C0D
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_SSH_REP_LOW_CS_EN       0x2C10
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_SSH_REP_HIGH_CS_DIS     0x2400
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_SSH_REP_MID_CS_DIS      0x240B
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_SSH_REP_LOW_CS_DIS      0x2416
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_HIGH_MPS_0_5   0x2032
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_MID_MPS_0_5    0x2024
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_LOW_MPS_0_5    0x202F
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_HIGH_MPS_1     0x2130
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_MID_MPS_1      0x2126
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_LOW_MPS_1      0x212D
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_HIGH_MPS_2     0x2236
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_MID_MPS_2      0x2220
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_LOW_MPS_2      0x222B
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_HIGH_MPS_4     0x2334
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_MID_MPS_4      0x2322
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_LOW_MPS_4      0x2329
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_HIGH_MPS_10    0x2737
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_MID_MPS_10     0x2721
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_REP_LOW_MPS_10     0x272A
#define AM_DEVICES_TEMPHUM25_CMD_MEAS_CONT_ART                0x2B32
#define AM_DEVICES_TEMPHUM25_CMD_CONT_MEAS_STOP               0x3093
#define AM_DEVICES_TEMPHUM25_CMD_FETCH_DATA                   0xE000
#define AM_DEVICES_TEMPHUM25_CMD_SOFT_RESET                   0x30A2
#define AM_DEVICES_TEMPHUM25_CMD_HEATER_ENABLE                0x306D
#define AM_DEVICES_TEMPHUM25_CMD_HEATER_DISABLE               0x3066
#define AM_DEVICES_TEMPHUM25_CMD_GET_SERIAL_NUM_0             0x3780
//! Alternate serial-number command word (unused by this driver; provided
//! for reference alongside _NUM_0, which the driver uses exclusively).
#define AM_DEVICES_TEMPHUM25_CMD_GET_SERIAL_NUM_1             0x3682
#define AM_DEVICES_TEMPHUM25_CMD_READ_STATUS                  0xF32D
#define AM_DEVICES_TEMPHUM25_CMD_CLEAR_STATUS                 0x3041
//! @}

//*****************************************************************************
//
//! Global type definitions.
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_TEMPHUM25_STATUS_SUCCESS,
    AM_DEVICES_TEMPHUM25_STATUS_ERROR,
    AM_DEVICES_TEMPHUM25_STATUS_INVALID_HANDLE,
    AM_DEVICES_TEMPHUM25_STATUS_INVALID_ARG,
    AM_DEVICES_TEMPHUM25_STATUS_CRC_ERROR,
    AM_DEVICES_TEMPHUM25_STATUS_IN_USE,
} am_devices_temphum25_status_t;

typedef enum
{
    AM_DEVICES_TEMPHUM25_FAIL_STEP_NONE = 0,
    AM_DEVICES_TEMPHUM25_FAIL_STEP_CMD_WRITE,
    AM_DEVICES_TEMPHUM25_FAIL_STEP_DATA_READ,
    AM_DEVICES_TEMPHUM25_FAIL_STEP_CRC_CHECK,
} am_devices_temphum25_fail_step_t;

typedef struct
{
    am_devices_temphum25_fail_step_t eStep;
    uint32_t ui32DriverStatus;
    uint32_t ui32HalStatus;
    uint16_t ui16Command;
    uint8_t  ui8I2cAddr;
    uint8_t  ui8RawData[6];
    bool     bRawDataValid;
    uint8_t  ui8CrcCalc[2];
    uint8_t  ui8CrcRx[2];
} am_devices_temphum25_last_error_t;

typedef struct
{
    void     *pIomHandle;
    uint8_t   ui8I2cAddr;
    uint32_t  ui32ResetGpio;
    uint32_t  ui32AlertGpio;
} am_devices_temphum25_config_t;

//*****************************************************************************
//
//! @brief Initialize a TempHum 25 configuration structure with defaults.
//!
//! @param psConfig - Pointer to the configuration structure to initialize.
//!
//! @note The caller must still set pIomHandle and, optionally, GPIO numbers.
//
//*****************************************************************************
extern void am_devices_temphum25_config_init(am_devices_temphum25_config_t *psConfig);

//*****************************************************************************
//
//! @brief Initialize the TempHum 25 driver.
//!
//! @param psConfig - Pointer to the device configuration structure.
//! @param ppHandle - Pointer to storage for the returned device handle.
//!
//! @return Status code.
//!
//! @note The IOM must already be initialized, configured for I2C, enabled,
//!       and have its pins configured before calling this function.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_init(am_devices_temphum25_config_t *psConfig,
                                          void **ppHandle);

//*****************************************************************************
//
//! @brief Release a TempHum 25 device handle.
//!
//! @param ppHandle - Pointer to the device handle pointer.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_term(void **ppHandle);

//*****************************************************************************
//
//! @brief Send a 16-bit command word to the sensor.
//!
//! @param pHandle - Device handle returned from init().
//! @param ui16Cmd - Command word to send.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_send_command(void *pHandle, uint16_t ui16Cmd);

//*****************************************************************************
//
//! @brief Start a measurement using the selected command word.
//!
//! @param pHandle - Device handle returned from init().
//! @param ui16MeasCmd - Measurement command word.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_start_measurement(void *pHandle, uint16_t ui16MeasCmd);

//*****************************************************************************
//
//! @brief Stop continuous measurement mode.
//!
//! @param pHandle - Device handle returned from init().
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_stop_measurement(void *pHandle);

//*****************************************************************************
//
//! @brief Read and decode a single-shot measurement result.
//!
//! Issue this after start_measurement() completes. The sensor returns six
//! bytes (temperature, CRC, humidity, CRC) in response to an I2C read header.
//!
//! @param pHandle - Device handle returned from init().
//! @param pfTemperature - Output temperature in degrees Celsius.
//! @param pfHumidity - Output relative humidity in percent.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_read_measurement(void *pHandle,
                                                      float *pfTemperature,
                                                      float *pfHumidity);

//*****************************************************************************
//
//! @brief Fetch and decode the latest periodic-mode measurement.
//!
//! Use this after a continuous/periodic measurement command. The driver sends
//! FETCH_DATA before reading the six-byte result frame.
//!
//! @param pHandle - Device handle returned from init().
//! @param pfTemperature - Output temperature in degrees Celsius.
//! @param pfHumidity - Output relative humidity in percent.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_fetch_measurement(void *pHandle,
                                                       float *pfTemperature,
                                                       float *pfHumidity);

//*****************************************************************************
//
//! @brief Perform a single-shot measurement using I2C clock stretching.
//!
//! Sends a clock-stretch-enabled (_CS_EN) measurement command, waits the
//! minimum 1 ms, then issues a direct I2C read. The SHT33 holds SCL low
//! until the conversion completes when clock stretching is enabled on both
//! the sensor command and the IOM (MI2CCFG.STRDIS = 0).
//!
//! @param pHandle - Device handle returned from init().
//! @param ui16MeasCmd - Measurement command word (use a _CS_EN variant).
//! @param pfTemperature - Output temperature in degrees Celsius.
//! @param pfHumidity - Output relative humidity in percent.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_measure_stretch(void *pHandle,
                                                     uint16_t ui16MeasCmd,
                                                     float *pfTemperature,
                                                     float *pfHumidity);

//*****************************************************************************
//
//! @brief Perform a single-shot measurement and return the result.
//!
//! This is a convenience wrapper around start_measurement() followed by
//! read_measurement(). Use a _CS_DIS command or allow the full conversion
//! delay before read_measurement(). For clock-stretch readout, use
//! am_devices_temphum25_measure_stretch() instead.
//!
//! @param pHandle - Device handle returned from init().
//! @param ui16MeasCmd - Measurement command word.
//! @param pfTemperature - Output temperature in degrees Celsius.
//! @param pfHumidity - Output relative humidity in percent.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_measure(void *pHandle,
                                             uint16_t ui16MeasCmd,
                                             float *pfTemperature,
                                             float *pfHumidity);

//*****************************************************************************
//
//! @brief Read the 32-bit device serial number.
//!
//! @param pHandle - Device handle returned from init().
//! @param pui32SerialNum - Output serial number.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_read_serial_num(void *pHandle,
                                                     uint32_t *pui32SerialNum);

//*****************************************************************************
//
//! @brief Read the 16-bit status register.
//!
//! @param pHandle - Device handle returned from init().
//! @param pui16Status - Output status register value.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_read_status(void *pHandle,
                                                 uint16_t *pui16Status);

//*****************************************************************************
//
//! @brief Clear the status register.
//!
//! @param pHandle - Device handle returned from init().
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_clear_status(void *pHandle);

//*****************************************************************************
//
//! @brief Enable the on-chip heater.
//!
//! @param pHandle - Device handle returned from init().
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_heater_enable(void *pHandle);

//*****************************************************************************
//
//! @brief Disable the on-chip heater.
//!
//! @param pHandle - Device handle returned from init().
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_heater_disable(void *pHandle);

//*****************************************************************************
//
//! @brief Perform a software reset of the sensor.
//!
//! @param pHandle - Device handle returned from init().
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_soft_reset(void *pHandle);

//*****************************************************************************
//
//! @brief Set the reset GPIO state, if configured.
//!
//! @param pHandle - Device handle returned from init().
//! @param bState - Desired reset pin state. true = deasserted/high, false = asserted/low.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_reset_pin_set(void *pHandle, bool bState);

//*****************************************************************************
//
//! @brief Toggle the reset GPIO to reset the sensor, if configured.
//!
//! @param pHandle - Device handle returned from init().
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_reset_device(void *pHandle);

//*****************************************************************************
//
//! @brief Read the alert GPIO state, if configured.
//!
//! @param pHandle - Device handle returned from init().
//! @param pbAlert - Output alert pin state.
//!
//! @return Status code.
//
//*****************************************************************************
extern uint32_t am_devices_temphum25_alert_get(void *pHandle, bool *pbAlert);

//*****************************************************************************
//
//! @brief Clear the stored driver last-error record.
//
//*****************************************************************************
extern void am_devices_temphum25_clear_last_error(void);

//*****************************************************************************
//
//! @brief Read the most recent driver error details.
//!
//! @param psError - Output structure. May be NULL to ignore.
//
//*****************************************************************************
extern void am_devices_temphum25_get_last_error(am_devices_temphum25_last_error_t *psError);

//*****************************************************************************
//
//! @brief Return a human-readable driver status name.
//
//*****************************************************************************
extern const char *am_devices_temphum25_status_name(uint32_t ui32Status);

//*****************************************************************************
//
//! @brief Return a human-readable failure-step name.
//
//*****************************************************************************
extern const char *am_devices_temphum25_fail_step_name(am_devices_temphum25_fail_step_t eStep);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_TEMPHUM25_H

//! @}
//
//*****************************************************************************
