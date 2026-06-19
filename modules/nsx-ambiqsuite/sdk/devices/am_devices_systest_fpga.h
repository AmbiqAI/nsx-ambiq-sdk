//*****************************************************************************
//
//! @file am_devices_systest_fpga.h
//!
//! @brief FPGA Test Signal Generator Device Driver
//!
//! @addtogroup devices_fpga FPGA Test Signal Generator Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a hardware abstraction layer for the FPGA-based
//! test signal generator board. The FPGA generates PDM audio signals, I2S clocks,
//! and DAC waveforms for testing Apollo MCU peripherals. Communication is via I2C.
//!
//! @section fpga_features Key Features
//!
//! 1. @b PDM @b Generator: Generates stereo PDM audio with configurable tone frequencies.
//! 2. @b I2S @b Controller: Configures I2S bit clock, word select, and monitors actual sample rate.
//! 3. @b DAC @b Waveform: Generates Sine, Triangle, Square, or fixed DC output via MCP4725.
//! 4. @b I2C @b Interface: Uses IOM in I2C mode for register access.
//!
//! @section fpga_usage Usage
//!
//! 1. Initialize FPGA device using am_devices_fpga_init()
//! 2. Configure PDM/I2S/DAC settings as needed
//! 3. Enable/disable outputs
//! 4. Deinitialize with am_devices_fpga_deinit()
//!
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.06.17 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_DEVICES_SYSTEST_FPGA_H
#define AM_DEVICES_SYSTEST_FPGA_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include "am_mcu_apollo.h"

//*****************************************************************************
//
// Device Constants
//
//*****************************************************************************
#define AM_DEVICES_FPGA_I2C_ADDR_DEFAULT    0x44
#define AM_DEVICES_FPGA_DEVICE_ID_EXPECTED  0x447A0147
#define AM_DEVICES_SYSTEST_FPGA_PIN_SETTLE_DELAY_US 50000

//*****************************************************************************
//
// FPGA Register Map
// I2C Address: 0x44 (Write: 0x88, Read: 0x89)
//
//*****************************************************************************

// Device ID Register (Read-Only, 32-bit)
#define AM_DEVICES_FPGA_REG_DEVICE_ID       0x00    // 0x00-0x03, RO, Default: 0x447A0147

// PDM Tone Generator Registers (Read-Write, 32-bit)
#define AM_DEVICES_FPGA_REG_PDM_TONE_L      0x10    // 0x10-0x13, RW, Left channel freq (Hz)
#define AM_DEVICES_FPGA_REG_PDM_TONE_R      0x14    // 0x14-0x17, RW, Right channel freq (Hz)
#define AM_DEVICES_FPGA_REG_PDM_ENABLE      0x1F    // RW, 8-bit, PDM output (0=Hi-Z, 1=output)

// I2S Configuration Registers
#define AM_DEVICES_FPGA_REG_I2S_BCLK        0x20    // 0x20-0x23, RW, 32-bit, Expected bit clock (Hz)
#define AM_DEVICES_FPGA_REG_I2S_WS          0x24    // 0x24-0x27, RW, 32-bit, Expected word select (Hz)
#define AM_DEVICES_FPGA_REG_I2S_BIT_DEPTH   0x28    // RW, 8-bit, Bits per channel (default 24)
#define AM_DEVICES_FPGA_REG_I2S_WORD_WIDTH  0x29    // RW, 8-bit, L+R total bits (default 24)
#define AM_DEVICES_FPGA_REG_I2S_TX_CLK_MODE 0x2A    // RW, 8-bit, TX clock mode (0=8kHz, 1=16kHz)
#define AM_DEVICES_FPGA_REG_I2S_TX_CLK_EN   0x2B    // RW, 8-bit, TX clock enable
#define AM_DEVICES_FPGA_REG_I2S_ENABLE      0x2C    // RW, 8-bit, I2S receiver enable (default 1)

// I2S Status Registers (Read-Only)
#define AM_DEVICES_FPGA_REG_I2S_ACTUAL_FS   0x30    // 0x30-0x33, RO, 32-bit, Measured sample rate (Hz)
#define AM_DEVICES_FPGA_REG_I2S_MAIN_TONE_L 0x34    // 0x34-0x37, RO, 32-bit, Detected main frequency (Hz)

// I2S Control Registers
#define AM_DEVICES_FPGA_REG_I2S_CLEAR       0x3C    // WO, 8-bit, Write 1 to clear status
#define AM_DEVICES_FPGA_REG_UART_LOOPBACK   0x3E    // RW, 8-bit, UART loopback (0=Hi-Z, 1=P1->R1)
#define AM_DEVICES_FPGA_REG_I2S_SD_LOOPBACK 0x3F    // RW, 8-bit, SD loopback (0=Hi-Z, 1=T4->N1)

// DAC Waveform Generator Registers
#define AM_DEVICES_FPGA_REG_DAC_WAVE_TYPE   0x40    // RW, 8-bit, Wave type (0-3)
#define AM_DEVICES_FPGA_REG_DAC_ENABLE      0x41    // RW, 8-bit, DAC output enable
#define AM_DEVICES_FPGA_REG_DAC_FREQ        0x42    // 0x42-0x43, RW, 16-bit, Output freq (1-1024 Hz)
#define AM_DEVICES_FPGA_REG_DAC_FIX_VALUE   0x45    // 0x45-0x46, RW, 16-bit, Fix value for MCP4725

//*****************************************************************************
//
// Enumerations
//
//*****************************************************************************

//
//! DAC waveform types
//
typedef enum
{
    AM_DEVICES_FPGA_DAC_WAVE_SINE     = 0,
    AM_DEVICES_FPGA_DAC_WAVE_TRIANGLE = 1,
    AM_DEVICES_FPGA_DAC_WAVE_SQUARE   = 2,
    AM_DEVICES_FPGA_DAC_WAVE_FIX      = 3
} am_devices_fpga_dac_wave_e;

//
//! I2S TX clock mode
//
typedef enum
{
    AM_DEVICES_FPGA_I2S_TX_CLK_8KHZ  = 0,
    AM_DEVICES_FPGA_I2S_TX_CLK_16KHZ = 1
} am_devices_fpga_i2s_tx_clk_e;

//
//! DAC power-down mode (for FIX_VALUE register)
//
typedef enum
{
    AM_DEVICES_FPGA_DAC_PD_NORMAL = 0,  // Normal operation
    AM_DEVICES_FPGA_DAC_PD_1K     = 1,  // 1kΩ to GND
    AM_DEVICES_FPGA_DAC_PD_100K   = 2,  // 100kΩ to GND
    AM_DEVICES_FPGA_DAC_PD_500K   = 3   // 500kΩ to GND
} am_devices_fpga_dac_pd_e;

//
//! Device status codes
//
typedef enum
{
    AM_DEVICES_FPGA_STATUS_SUCCESS = 0,
    AM_DEVICES_FPGA_STATUS_ERROR,
    AM_DEVICES_FPGA_STATUS_INVALID_HANDLE,
    AM_DEVICES_FPGA_STATUS_INVALID_ARG,
    AM_DEVICES_FPGA_STATUS_IOM_ERROR,
    AM_DEVICES_FPGA_STATUS_DEVICE_ID_MISMATCH
} am_devices_fpga_status_e;

//*****************************************************************************
//
// Configuration Structures
//
//*****************************************************************************

//
//! Device initialization configuration
//
typedef struct
{
    uint32_t ui32IomModule;     // IOM module number (0-7)
    uint32_t ui32I2cAddress;    // I2C address (default 0x44)
    uint32_t ui32ClockFreq;     // I2C clock frequency (e.g., AM_HAL_IOM_400KHZ)
} am_devices_fpga_config_t;

//
//! PDM tone generator configuration
//
typedef struct
{
    uint32_t ui32ToneFreqL;     // Left channel frequency (Hz), default 1000
    uint32_t ui32ToneFreqR;     // Right channel frequency (Hz), default 1000
} am_devices_fpga_pdm_config_t;

//
//! I2S configuration
//
typedef struct
{
    uint32_t ui32Bclk;          // Expected bit clock (Hz)
    uint32_t ui32Ws;            // Expected word select (Hz)
    uint8_t  ui8BitDepth;       // Bits per channel (default 24)
    uint8_t  ui8WordWidth;      // L+R total bits (default 24)
    am_devices_fpga_i2s_tx_clk_e eTxClkMode;  // TX clock mode
    bool     bSdLoopback;       // SD loopback enable (0=Hi-Z, 1=T4->N1)
} am_devices_fpga_i2s_config_t;

//
//! DAC waveform generator configuration
//
typedef struct
{
    am_devices_fpga_dac_wave_e eWaveType;  // Wave type (Sine/Triangle/Square/Fix)
    uint16_t ui16Frequency;                 // Output frequency (1-1024 Hz)
} am_devices_fpga_dac_config_t;

//
//! DAC fix value configuration (for MCP4725 direct control)
//! Format: [15:14]=Reserved(00), [13:12]=PD[1:0], [11:0]=D[11:0]
//
typedef struct
{
    am_devices_fpga_dac_pd_e ePowerDown;   // Power-down mode
    uint16_t ui16DacValue;                  // 12-bit DAC value (0x000 - 0xFFF)
} am_devices_fpga_dac_fix_config_t;

//*****************************************************************************
//
// Public Function Prototypes
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Initialize FPGA device driver
//!
//! @param pConfig - Pointer to device configuration
//! @param ppHandle - Returns device handle
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_init(am_devices_fpga_config_t *pConfig,
                                     void **ppHandle);

//*****************************************************************************
//
//! @brief Deinitialize FPGA device driver
//!
//! @param pHandle - Device handle
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_deinit(void *pHandle);

//*****************************************************************************
//
//! @brief Read FPGA device ID
//!
//! @param pHandle - Device handle
//! @param pui32DeviceId - Returns 32-bit device ID (expected: 0x447A0147)
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_read_device_id(void *pHandle,
                                               uint32_t *pui32DeviceId);

//*****************************************************************************
//
// PDM Control Functions
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Configure PDM tone generator
//!
//! @param pHandle - Device handle
//! @param pConfig - PDM configuration (left/right channel frequencies)
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_pdm_configure(void *pHandle,
                                              am_devices_fpga_pdm_config_t *pConfig);

//*****************************************************************************
//
//! @brief Enable/disable PDM output
//!
//! @param pHandle - Device handle
//! @param bEnable - true to enable PDM output, false to set Hi-Z
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_pdm_enable(void *pHandle, bool bEnable);

//*****************************************************************************
//
// I2S Control Functions
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Configure I2S controller
//!
//! @param pHandle - Device handle
//! @param pConfig - I2S configuration
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_i2s_configure(void *pHandle,
                                              am_devices_fpga_i2s_config_t *pConfig);

//*****************************************************************************
//
//! @brief Enable/disable I2S receiver
//!
//! @param pHandle - Device handle
//! @param bEnable - true to enable, false to disable
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_i2s_enable(void *pHandle, bool bEnable);

//*****************************************************************************
//
//! @brief Enable/disable I2S TX clock
//!
//! @param pHandle - Device handle
//! @param bEnable - true to enable, false to disable
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_i2s_tx_clk_enable(void *pHandle, bool bEnable);

//*****************************************************************************
//
//! @brief Enable/disable UART loopback (P1 -> R1)
//!
//! @param pHandle - Device handle
//! @param bEnable - true to enable, false to disable (Hi-Z)
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_uart_loopback_enable(void *pHandle, bool bEnable);

//*****************************************************************************
//
//! @brief Read actual measured sample rate
//!
//! @param pHandle - Device handle
//! @param pui32ActualFs - Returns measured sample rate in Hz
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_i2s_read_actual_fs(void *pHandle,
                                                   uint32_t *pui32ActualFs);

//*****************************************************************************
//
//! @brief Read detected main tone frequency
//!
//! @param pHandle - Device handle
//! @param pui32MainTone - Returns detected frequency in Hz
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_i2s_read_main_tone(void *pHandle,
                                                   uint32_t *pui32MainTone);

//*****************************************************************************
//
//! @brief Clear I2S status registers
//!
//! @param pHandle - Device handle
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_i2s_clear_status(void *pHandle);

//*****************************************************************************
//
// DAC Control Functions
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Configure DAC waveform generator
//!
//! @param pHandle - Device handle
//! @param pConfig - DAC configuration (wave type and frequency)
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_dac_configure(void *pHandle,
                                              am_devices_fpga_dac_config_t *pConfig);

//*****************************************************************************
//
//! @brief Enable/disable DAC output
//!
//! @param pHandle - Device handle
//! @param bEnable - true to enable, false to disable
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_dac_enable(void *pHandle, bool bEnable);

//*****************************************************************************
//
//! @brief Set DAC fix value (direct MCP4725 control)
//!
//! @param pHandle - Device handle
//! @param pConfig - Fix value configuration (power-down mode and 12-bit value)
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_dac_set_fix_value(void *pHandle,
                                                  am_devices_fpga_dac_fix_config_t *pConfig);

//*****************************************************************************
//
// Low-level Register Access Functions
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Write single byte to FPGA register
//!
//! @param pHandle - Device handle
//! @param ui8Reg - Register address
//! @param ui8Value - Value to write
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_reg_write(void *pHandle,
                                          uint8_t ui8Reg,
                                          uint8_t ui8Value);

//*****************************************************************************
//
//! @brief Write 16-bit value to FPGA register (Big-Endian)
//!
//! @param pHandle - Device handle
//! @param ui8Reg - Register address
//! @param ui16Value - 16-bit value to write
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_reg_write16(void *pHandle,
                                            uint8_t ui8Reg,
                                            uint16_t ui16Value);

//*****************************************************************************
//
//! @brief Write 32-bit value to FPGA register (Big-Endian)
//!
//! @param pHandle - Device handle
//! @param ui8Reg - Register address
//! @param ui32Value - 32-bit value to write
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_reg_write32(void *pHandle,
                                            uint8_t ui8Reg,
                                            uint32_t ui32Value);

//*****************************************************************************
//
//! @brief Read bytes from FPGA register
//!
//! @param pHandle - Device handle
//! @param ui8Reg - Register address
//! @param pui8Data - Buffer to store read data
//! @param ui32Len - Number of bytes to read
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_reg_read(void *pHandle,
                                         uint8_t ui8Reg,
                                         uint8_t *pui8Data,
                                         uint32_t ui32Len);

//*****************************************************************************
//
//! @brief Read 32-bit value from FPGA register (Big-Endian)
//!
//! @param pHandle - Device handle
//! @param ui8Reg - Register address
//! @param pui32Value - Returns 32-bit value
//!
//! @return Status code
//
//*****************************************************************************
extern uint32_t am_devices_fpga_reg_read32(void *pHandle,
                                           uint8_t ui8Reg,
                                           uint32_t *pui32Value);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_SYSTEST_FPGA_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
