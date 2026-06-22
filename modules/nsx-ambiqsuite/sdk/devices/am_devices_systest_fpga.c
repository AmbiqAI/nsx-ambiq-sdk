//*****************************************************************************
//
//! @file am_devices_systest_fpga.c
//!
//! @brief FPGA Test Signal Generator Device Driver
//!
//! @addtogroup devices_fpga FPGA Test Signal Generator Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a hardware abstraction layer for the FPGA-based
//! test signal generator board. The FPGA generates PDM audio signals, I2S clocks,
//! and DAC waveforms for testing Apollo MCU peripherals.
//!
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
#include "am_mcu_apollo.h"
#include "am_devices_systest_fpga.h"

//*****************************************************************************
//
// Private Definitions
//
//*****************************************************************************

#define AM_DEVICES_FPGA_MAX_INSTANCES   4

//*****************************************************************************
//
// Private Types
//
//*****************************************************************************

//
// Device state structure
//
typedef struct
{
    bool     bInUse;
    void     *pIomHandle;
    uint32_t ui32IomModule;
    uint32_t ui32I2cAddress;
    am_hal_iom_buffer(16) sBuffer;
} am_devices_fpga_t;

//*****************************************************************************
//
// Private Variables
//
//*****************************************************************************

static am_devices_fpga_t g_sFpgaDevices[AM_DEVICES_FPGA_MAX_INSTANCES];

//*****************************************************************************
//
// IOM Configuration
//
//*****************************************************************************
static am_hal_iom_config_t g_sIomConfig =
{
    .eInterfaceMode     = AM_HAL_IOM_I2C_MODE,
    .ui32ClockFreq      = AM_HAL_IOM_400KHZ,
    .pNBTxnBuf          = NULL,
    .ui32NBTxnBufLength = 0
};

//*****************************************************************************
//
// Private Functions
//
//*****************************************************************************

//*****************************************************************************
//
// Validate device handle
//
//*****************************************************************************
static inline am_devices_fpga_t*
fpga_handle_validate(void *pHandle)
{
    am_devices_fpga_t *pDevice = (am_devices_fpga_t *)pHandle;

    if ((pDevice == NULL) || (!pDevice->bInUse))
    {
        return NULL;
    }

    return pDevice;
}

//*****************************************************************************
//
// Public Functions
//
//*****************************************************************************

//*****************************************************************************
//
// Initialize FPGA device driver
//
//*****************************************************************************
uint32_t
am_devices_fpga_init(am_devices_fpga_config_t *pConfig, void **ppHandle)
{
    uint32_t ui32Status;
    am_devices_fpga_t *pDevice = NULL;

    if ((pConfig == NULL) || (ppHandle == NULL))
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_ARG;
    }

    //
    // Find an available device slot
    //
    for (uint32_t i = 0; i < AM_DEVICES_FPGA_MAX_INSTANCES; i++)
    {
        if (!g_sFpgaDevices[i].bInUse)
        {
            pDevice = &g_sFpgaDevices[i];
            break;
        }
    }

    if (pDevice == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_ERROR;
    }

    //
    // Initialize IOM
    //
    ui32Status = am_hal_iom_initialize(pConfig->ui32IomModule, &pDevice->pIomHandle);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_FPGA_STATUS_IOM_ERROR;
    }

    //
    // Power on IOM
    //
    ui32Status = am_hal_iom_power_ctrl(pDevice->pIomHandle, AM_HAL_SYSCTRL_WAKE, false);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        am_hal_iom_uninitialize(pDevice->pIomHandle);
        return AM_DEVICES_FPGA_STATUS_IOM_ERROR;
    }

    //
    // Configure IOM
    //
    g_sIomConfig.ui32ClockFreq = pConfig->ui32ClockFreq ? pConfig->ui32ClockFreq : AM_HAL_IOM_400KHZ;

    ui32Status = am_hal_iom_configure(pDevice->pIomHandle, &g_sIomConfig);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        am_hal_iom_uninitialize(pDevice->pIomHandle);
        return AM_DEVICES_FPGA_STATUS_IOM_ERROR;
    }

    //
    // Enable IOM
    //
    ui32Status = am_hal_iom_enable(pDevice->pIomHandle);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        am_hal_iom_uninitialize(pDevice->pIomHandle);
        return AM_DEVICES_FPGA_STATUS_IOM_ERROR;
    }

    //
    // Store device configuration
    //
    pDevice->ui32IomModule = pConfig->ui32IomModule;
    pDevice->ui32I2cAddress = pConfig->ui32I2cAddress ? pConfig->ui32I2cAddress : AM_DEVICES_FPGA_I2C_ADDR_DEFAULT;
    pDevice->bInUse = true;

    *ppHandle = pDevice;

    return AM_DEVICES_FPGA_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Deinitialize FPGA device driver
//
//*****************************************************************************
uint32_t
am_devices_fpga_deinit(void *pHandle)
{
    am_devices_fpga_t *pDevice = fpga_handle_validate(pHandle);

    if (pDevice == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_HANDLE;
    }

    //
    // Disable IOM
    //
    am_hal_iom_disable(pDevice->pIomHandle);

    //
    // Power off IOM
    //
    am_hal_iom_power_ctrl(pDevice->pIomHandle, AM_HAL_SYSCTRL_DEEPSLEEP, false);

    //
    // Uninitialize IOM
    //
    am_hal_iom_uninitialize(pDevice->pIomHandle);

    //
    // Clear device state
    //
    pDevice->pIomHandle = NULL;
    pDevice->bInUse = false;

    return AM_DEVICES_FPGA_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Low-level Register Access Functions
//
//*****************************************************************************

//*****************************************************************************
//
// Write single byte to FPGA register
//
//*****************************************************************************
uint32_t
am_devices_fpga_reg_write(void *pHandle, uint8_t ui8Reg, uint8_t ui8Value)
{
    am_devices_fpga_t *pDevice = fpga_handle_validate(pHandle);
    am_hal_iom_transfer_t sTransaction;

    if (pDevice == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_HANDLE;
    }

    pDevice->sBuffer.bytes[0] = ui8Reg;
    pDevice->sBuffer.bytes[1] = ui8Value;

    sTransaction.uPeerInfo.ui32I2CDevAddr = pDevice->ui32I2cAddress;
    sTransaction.ui32InstrLen      = 0;
    sTransaction.ui64Instr         = 0;
    sTransaction.eDirection        = AM_HAL_IOM_TX;
    sTransaction.ui32NumBytes      = 2;
    sTransaction.pui32TxBuffer     = pDevice->sBuffer.words;
    sTransaction.bContinue         = false;
    sTransaction.ui8RepeatCount    = 0;
    sTransaction.ui32PauseCondition = 0;
    sTransaction.ui32StatusSetClr  = 0;

    if (am_hal_iom_blocking_transfer(pDevice->pIomHandle, &sTransaction) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_FPGA_STATUS_IOM_ERROR;
    }

    return AM_DEVICES_FPGA_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Write 16-bit value to FPGA register (Big-Endian)
//
//*****************************************************************************
uint32_t
am_devices_fpga_reg_write16(void *pHandle, uint8_t ui8Reg, uint16_t ui16Value)
{
    am_devices_fpga_t *pDevice = fpga_handle_validate(pHandle);
    am_hal_iom_transfer_t sTransaction;

    if (pDevice == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_HANDLE;
    }

    pDevice->sBuffer.bytes[0] = ui8Reg;
    pDevice->sBuffer.bytes[1] = (ui16Value >> 8) & 0xFF;  // High byte first
    pDevice->sBuffer.bytes[2] = ui16Value & 0xFF;         // Low byte

    sTransaction.uPeerInfo.ui32I2CDevAddr = pDevice->ui32I2cAddress;
    sTransaction.ui32InstrLen      = 0;
    sTransaction.ui64Instr         = 0;
    sTransaction.eDirection        = AM_HAL_IOM_TX;
    sTransaction.ui32NumBytes      = 3;
    sTransaction.pui32TxBuffer     = pDevice->sBuffer.words;
    sTransaction.bContinue         = false;
    sTransaction.ui8RepeatCount    = 0;
    sTransaction.ui32PauseCondition = 0;
    sTransaction.ui32StatusSetClr  = 0;

    if (am_hal_iom_blocking_transfer(pDevice->pIomHandle, &sTransaction) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_FPGA_STATUS_IOM_ERROR;
    }

    return AM_DEVICES_FPGA_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Write 32-bit value to FPGA register (Big-Endian)
//
//*****************************************************************************
uint32_t
am_devices_fpga_reg_write32(void *pHandle, uint8_t ui8Reg, uint32_t ui32Value)
{
    am_devices_fpga_t *pDevice = fpga_handle_validate(pHandle);
    am_hal_iom_transfer_t sTransaction;

    if (pDevice == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_HANDLE;
    }

    pDevice->sBuffer.bytes[0] = ui8Reg;
    pDevice->sBuffer.bytes[1] = (ui32Value >> 24) & 0xFF;
    pDevice->sBuffer.bytes[2] = (ui32Value >> 16) & 0xFF;
    pDevice->sBuffer.bytes[3] = (ui32Value >> 8) & 0xFF;
    pDevice->sBuffer.bytes[4] = ui32Value & 0xFF;

    sTransaction.uPeerInfo.ui32I2CDevAddr = pDevice->ui32I2cAddress;
    sTransaction.ui32InstrLen      = 0;
    sTransaction.ui64Instr         = 0;
    sTransaction.eDirection        = AM_HAL_IOM_TX;
    sTransaction.ui32NumBytes      = 5;
    sTransaction.pui32TxBuffer     = pDevice->sBuffer.words;
    sTransaction.bContinue         = false;
    sTransaction.ui8RepeatCount    = 0;
    sTransaction.ui32PauseCondition = 0;
    sTransaction.ui32StatusSetClr  = 0;

    if (am_hal_iom_blocking_transfer(pDevice->pIomHandle, &sTransaction) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_FPGA_STATUS_IOM_ERROR;
    }

    return AM_DEVICES_FPGA_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Read bytes from FPGA register
//
//*****************************************************************************
uint32_t
am_devices_fpga_reg_read(void *pHandle, uint8_t ui8Reg, uint8_t *pui8Data, uint32_t ui32Len)
{
    am_devices_fpga_t *pDevice = fpga_handle_validate(pHandle);
    am_hal_iom_transfer_t sTransaction;
    uint32_t ui32Status;

    if (pDevice == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_HANDLE;
    }

    if ((pui8Data == NULL) || (ui32Len == 0))
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_ARG;
    }

    //
    // Write register address with repeated start
    //
    pDevice->sBuffer.bytes[0] = ui8Reg;

    sTransaction.uPeerInfo.ui32I2CDevAddr = pDevice->ui32I2cAddress;
    sTransaction.ui32InstrLen      = 0;
    sTransaction.ui64Instr         = 0;
    sTransaction.eDirection        = AM_HAL_IOM_TX;
    sTransaction.ui32NumBytes      = 1;
    sTransaction.pui32TxBuffer     = pDevice->sBuffer.words;
    sTransaction.bContinue         = true;  // Repeated start
    sTransaction.ui8RepeatCount    = 0;
    sTransaction.ui32PauseCondition = 0;
    sTransaction.ui32StatusSetClr  = 0;

    ui32Status = am_hal_iom_blocking_transfer(pDevice->pIomHandle, &sTransaction);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_FPGA_STATUS_IOM_ERROR;
    }

    //
    // Read data
    //
    sTransaction.eDirection    = AM_HAL_IOM_RX;
    sTransaction.ui32NumBytes  = ui32Len;
    sTransaction.pui32RxBuffer = (uint32_t *)pui8Data;
    sTransaction.bContinue     = false;

    ui32Status = am_hal_iom_blocking_transfer(pDevice->pIomHandle, &sTransaction);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_FPGA_STATUS_IOM_ERROR;
    }

    return AM_DEVICES_FPGA_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Read 32-bit value from FPGA register (Big-Endian)
//
//*****************************************************************************
uint32_t
am_devices_fpga_reg_read32(void *pHandle, uint8_t ui8Reg, uint32_t *pui32Value)
{
    uint8_t ui8Data[4];
    uint32_t ui32Status;

    if (pui32Value == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_ARG;
    }

    ui32Status = am_devices_fpga_reg_read(pHandle, ui8Reg, ui8Data, 4);
    if (ui32Status != AM_DEVICES_FPGA_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    *pui32Value = ((uint32_t)ui8Data[0] << 24) |
                  ((uint32_t)ui8Data[1] << 16) |
                  ((uint32_t)ui8Data[2] << 8)  |
                  ((uint32_t)ui8Data[3]);

    return AM_DEVICES_FPGA_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Read FPGA device ID
//
//*****************************************************************************
uint32_t
am_devices_fpga_read_device_id(void *pHandle, uint32_t *pui32DeviceId)
{
    return am_devices_fpga_reg_read32(pHandle, AM_DEVICES_FPGA_REG_DEVICE_ID, pui32DeviceId);
}

//*****************************************************************************
//
// PDM Control Functions
//
//*****************************************************************************

//*****************************************************************************
//
// Configure PDM tone generator
//
//*****************************************************************************
uint32_t
am_devices_fpga_pdm_configure(void *pHandle, am_devices_fpga_pdm_config_t *pConfig)
{
    uint32_t ui32Status;

    if (pConfig == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_ARG;
    }

    //
    // Write left channel frequency
    //
    ui32Status = am_devices_fpga_reg_write32(pHandle,
                                             AM_DEVICES_FPGA_REG_PDM_TONE_L,
                                             pConfig->ui32ToneFreqL);
    if (ui32Status != AM_DEVICES_FPGA_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Write right channel frequency
    //
    ui32Status = am_devices_fpga_reg_write32(pHandle,
                                             AM_DEVICES_FPGA_REG_PDM_TONE_R,
                                             pConfig->ui32ToneFreqR);

    return ui32Status;
}

//*****************************************************************************
//
// Enable/disable PDM output
//
//*****************************************************************************
uint32_t
am_devices_fpga_pdm_enable(void *pHandle, bool bEnable)
{
    return am_devices_fpga_reg_write(pHandle,
                                     AM_DEVICES_FPGA_REG_PDM_ENABLE,
                                     bEnable ? 1 : 0);
}

//*****************************************************************************
//
// I2S Control Functions
//
//*****************************************************************************

//*****************************************************************************
//
// Configure I2S controller
//
//*****************************************************************************
uint32_t
am_devices_fpga_i2s_configure(void *pHandle, am_devices_fpga_i2s_config_t *pConfig)
{
    uint32_t ui32Status;

    if (pConfig == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_ARG;
    }

    //
    // Write bit clock expectation
    //
    ui32Status = am_devices_fpga_reg_write32(pHandle,
                                             AM_DEVICES_FPGA_REG_I2S_BCLK,
                                             pConfig->ui32Bclk);
    if (ui32Status != AM_DEVICES_FPGA_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Write word select expectation
    //
    ui32Status = am_devices_fpga_reg_write32(pHandle,
                                             AM_DEVICES_FPGA_REG_I2S_WS,
                                             pConfig->ui32Ws);
    if (ui32Status != AM_DEVICES_FPGA_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Write bit depth
    //
    ui32Status = am_devices_fpga_reg_write(pHandle,
                                           AM_DEVICES_FPGA_REG_I2S_BIT_DEPTH,
                                           pConfig->ui8BitDepth);
    if (ui32Status != AM_DEVICES_FPGA_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Write word width
    //
    ui32Status = am_devices_fpga_reg_write(pHandle,
                                           AM_DEVICES_FPGA_REG_I2S_WORD_WIDTH,
                                           pConfig->ui8WordWidth);
    if (ui32Status != AM_DEVICES_FPGA_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Write TX clock mode
    //
    ui32Status = am_devices_fpga_reg_write(pHandle,
                                           AM_DEVICES_FPGA_REG_I2S_TX_CLK_MODE,
                                           (uint8_t)pConfig->eTxClkMode);
    if (ui32Status != AM_DEVICES_FPGA_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Write SD loopback setting
    //
    ui32Status = am_devices_fpga_reg_write(pHandle,
                                           AM_DEVICES_FPGA_REG_I2S_SD_LOOPBACK,
                                           pConfig->bSdLoopback ? 1 : 0);

    return ui32Status;
}

//*****************************************************************************
//
// Enable/disable I2S receiver
//
//*****************************************************************************
uint32_t
am_devices_fpga_i2s_enable(void *pHandle, bool bEnable)
{
    return am_devices_fpga_reg_write(pHandle,
                                     AM_DEVICES_FPGA_REG_I2S_ENABLE,
                                     bEnable ? 1 : 0);
}

//*****************************************************************************
//
// Enable/disable I2S TX clock
//
//*****************************************************************************
uint32_t
am_devices_fpga_i2s_tx_clk_enable(void *pHandle, bool bEnable)
{
    return am_devices_fpga_reg_write(pHandle,
                                     AM_DEVICES_FPGA_REG_I2S_TX_CLK_EN,
                                     bEnable ? 1 : 0);
}

//*****************************************************************************
//
// Enable/disable UART loopback (P1 -> R1)
//
//*****************************************************************************
uint32_t
am_devices_fpga_uart_loopback_enable(void *pHandle, bool bEnable)
{
    return am_devices_fpga_reg_write(pHandle,
                                     AM_DEVICES_FPGA_REG_UART_LOOPBACK,
                                     bEnable ? 1 : 0);
}

//*****************************************************************************
//
// Read actual measured sample rate
//
//*****************************************************************************
uint32_t
am_devices_fpga_i2s_read_actual_fs(void *pHandle, uint32_t *pui32ActualFs)
{
    return am_devices_fpga_reg_read32(pHandle,
                                      AM_DEVICES_FPGA_REG_I2S_ACTUAL_FS,
                                      pui32ActualFs);
}

//*****************************************************************************
//
// Read detected main tone frequency
//
//*****************************************************************************
uint32_t
am_devices_fpga_i2s_read_main_tone(void *pHandle, uint32_t *pui32MainTone)
{
    return am_devices_fpga_reg_read32(pHandle,
                                      AM_DEVICES_FPGA_REG_I2S_MAIN_TONE_L,
                                      pui32MainTone);
}

//*****************************************************************************
//
// Clear I2S status registers
//
//*****************************************************************************
uint32_t
am_devices_fpga_i2s_clear_status(void *pHandle)
{
    return am_devices_fpga_reg_write(pHandle,
                                     AM_DEVICES_FPGA_REG_I2S_CLEAR,
                                     1);
}

//*****************************************************************************
//
// DAC Control Functions
//
//*****************************************************************************

//*****************************************************************************
//
// Configure DAC waveform generator
//
//*****************************************************************************
uint32_t
am_devices_fpga_dac_configure(void *pHandle, am_devices_fpga_dac_config_t *pConfig)
{
    uint32_t ui32Status;

    if (pConfig == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_ARG;
    }

    //
    // Write wave type
    //
    ui32Status = am_devices_fpga_reg_write(pHandle,
                                           AM_DEVICES_FPGA_REG_DAC_WAVE_TYPE,
                                           (uint8_t)pConfig->eWaveType);
    if (ui32Status != AM_DEVICES_FPGA_STATUS_SUCCESS)
    {
        return ui32Status;
    }

    //
    // Write frequency (16-bit, Big-Endian)
    //
    ui32Status = am_devices_fpga_reg_write16(pHandle,
                                             AM_DEVICES_FPGA_REG_DAC_FREQ,
                                             pConfig->ui16Frequency);

    return ui32Status;
}

//*****************************************************************************
//
// Enable/disable DAC output
//
//*****************************************************************************
uint32_t
am_devices_fpga_dac_enable(void *pHandle, bool bEnable)
{
    return am_devices_fpga_reg_write(pHandle,
                                     AM_DEVICES_FPGA_REG_DAC_ENABLE,
                                     bEnable ? 1 : 0);
}

//*****************************************************************************
//
// Set DAC fix value (direct MCP4725 control)
//
//*****************************************************************************
uint32_t
am_devices_fpga_dac_set_fix_value(void *pHandle, am_devices_fpga_dac_fix_config_t *pConfig)
{
    uint16_t ui16Value;

    if (pConfig == NULL)
    {
        return AM_DEVICES_FPGA_STATUS_INVALID_ARG;
    }

    //
    // Format: [15:14]=Reserved(00), [13:12]=PD[1:0], [11:0]=D[11:0]
    //
    ui16Value = ((uint16_t)(pConfig->ePowerDown & 0x03) << 12) |
                (pConfig->ui16DacValue & 0x0FFF);

    return am_devices_fpga_reg_write16(pHandle,
                                       AM_DEVICES_FPGA_REG_DAC_FIX_VALUE,
                                       ui16Value);
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
