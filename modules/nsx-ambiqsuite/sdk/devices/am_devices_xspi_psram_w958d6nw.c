//*****************************************************************************
//
//! @file am_devices_xspi_psram_w958d6nw.c
//!
//! @brief Winbond W958D6NW xSPI PSRAM driver.
//!
//! @addtogroup devices_xspi_psram_w958d6nw W958D6NW xSPI PSRAM Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a hardware abstraction layer for the Winbond
//!          W958D6NW HyperBus-Extend-IO PSRAM device. It configures xSPIMC
//!          command/address/data phases for 16-bit DDR PSRAM access, validates
//!          the device ID registers, programs the CR0 latency settings, and
//!          supports AIP memory mapping.
//!
//! @section devices_xspi_psram_w958d6nw_features Key Features
//!
//! 1. @b HyperBus-Extend-IO @b Support: 16-bit DDR data path.
//! 2. @b Register @b Access: ID0, ID1, CR0, and CR1 read/write flow.
//! 3. @b Latency @b Configuration: Frequency based CR0 initial latency setup.
//! 4. @b AIP @b Mode: Address-in-place memory mapping support.
//! 5. @b Power @b Management: Hybrid Sleep entry and exit support.
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
#include "am_mcu_apollo.h"
#include "am_devices_xspi_psram_w958d6nw.h"
#include "am_util.h"
#include "am_bsp.h"

//*****************************************************************************
//
//! @name Local definitions
//! @{
//
//*****************************************************************************
#define XSPI_W958D6NW_BASE_FREQUENCY       25000000
#define AM_DEVICES_XSPI_PSRAM_TIMEOUT      1000000

//! @}

//*****************************************************************************
//
//! @name Local type definitions
//! @{
//
//*****************************************************************************
typedef struct
{
    void                                  *pBus;
    am_devices_xspi_psram_w958d6nw_regs_t  sRegisters;
    am_devices_xspi_info_t                 sDeviceInfo;
    am_devices_xspi_hal_config_t           sDeviceCfgs;
    am_hal_xspimc_dev_id                   sDeviceID;
    am_hal_xspimc_xfer_t                   sXfer;
    uint8_t                                ui8LpEntryData[2];
    uint8_t                                ui8LpExitData[2];
} w958d6nw_state_t;

typedef char w958d6nw_state_size_check[
    (sizeof(w958d6nw_state_t) <= AM_DEVICES_XSPI_PSRAM_W958D6NW_GLOBAL_RAM_SIZE) ? 1 : -1];

//! @}

//*****************************************************************************
//
//! @name Local globals
//! @{
//
//*****************************************************************************
static const am_hal_xspimc_dev_cfg_t sDevCfgW958D6NWDefault =
{
    .ui32Freq        = XSPI_W958D6NW_BASE_FREQUENCY,
    .eIOMode         = AM_HAL_XSPIMC_IO_OCTAL,
    .eDataRate       = AM_HAL_XSPIMC_DATA_RATE_DUAL,
    .eCpp            = AM_HAL_XSPIMC_CPP_MODE_0,
    .bDQS            = true,
    .ui32ReadCmd     = AM_DEVICES_XSPI_PSRAM_W958D6NW_DDR_READ,
    .ui32WriteCmd    = AM_DEVICES_XSPI_PSRAM_W958D6NW_DDR_WRITE,
    .ui8CmdLen       = 2,
    .ui8AddrLen      = 4,
    .ui8AddrAlign    = 1,
    .ui8RxDummy      = 14,
    .ui8TxDummy      = 14,
    .ui8DynTxDummy   = 14,
    .ui32MemBoundary = AM_DEVICES_XSPI_PSRAM_W958D6NW_PAGE_SIZE,
    .sCETiming       =
    {
        .ui32TCEMaxAct    = 4000,
        .ui32TCESetupHold = 4,
        .ui32TCEMinInact  = 6,
    },
    .eDeviceType = AM_HAL_XSPIMC_HYPERRAM_PSRAM_DEVICE,
};

//! @}

//*****************************************************************************
//
//! @brief Convert a CR0 latency code to clock cycles.
//!
//! @param eILC is the CR0 initial latency code.
//! @param pui8Latency is the returned latency in clock cycles.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_get_latency_cycles(am_devices_xspi_psram_w958d6nw_ilc_e eILC,
                            uint8_t *pui8Latency)
{
    switch (eILC)
    {
        case AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_3:
            *pui8Latency = 3;
            break;
        case AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_4:
            *pui8Latency = 4;
            break;
        case AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_5:
            *pui8Latency = 5;
            break;
        case AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_6:
            *pui8Latency = 6;
            break;
        case AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_7:
            *pui8Latency = 7;
            break;
        default:
            return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Select a safe CR0 latency code for the requested clock.
//!
//! @param ui32Freq is the xSPI device clock frequency in Hz.
//! @param peILC is the returned initial latency code.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_get_latency_from_freq(uint32_t ui32Freq,
                               am_devices_xspi_psram_w958d6nw_ilc_e *peILC)
{
// #### INTERNAL BEGIN ####
#if defined(ATOMIQ11X_FPGA)
    *peILC = AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_3;
    return AM_DEVICES_XSPI_STATUS_SUCCESS;
#endif
// #### INTERNAL END ####

    if (ui32Freq <= 85000000)
    {
        *peILC = AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_3;
    }
    else if (ui32Freq > 85000000 && ui32Freq <= 104000000)
    {
        *peILC = AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_4;
    }
    else if (ui32Freq > 104000000 && ui32Freq <= 133000000)
    {
        *peILC = AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_5;
    }
    else if (ui32Freq > 133000000 && ui32Freq <= 166000000)
    {
        *peILC = AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_6;
    }
    else if (ui32Freq > 166000000 && ui32Freq <= 200000000)
    {
        *peILC = AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_7;
    }
    else
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set the software register image to datasheet reset defaults.
//!
//! @param pState is the W958D6NW device state.
//
//*****************************************************************************
static void
w958d6nw_default_regs_set(w958d6nw_state_t *pState)
{
    memset(&pState->sRegisters, 0, sizeof(pState->sRegisters));

    pState->sRegisters.CR0_b.DPD           = 1;
    pState->sRegisters.CR0_b.DS            = AM_DEVICES_XSPI_PSRAM_W958D6NW_DS_34_OHM_0;
    pState->sRegisters.CR0_b.ILC           = AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_7;
    pState->sRegisters.CR0_b.FIXED_LATENCY = 1;
    pState->sRegisters.CR0_b.HYBRID_BURST  = 1;
    pState->sRegisters.CR0_b.BL            = AM_DEVICES_XSPI_PSRAM_W958D6NW_BL_32;

    pState->sRegisters.CR1_b.CLOCK_TYPE    = 1;
    pState->sRegisters.CR1_b.PAR           = AM_DEVICES_XSPI_PSRAM_W958D6NW_PAR_FULL;
}

//*****************************************************************************
//
//! @brief Set default xSPIMC device configuration.
//!
//! @param pState is the W958D6NW device state.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_default_devcfg_set(w958d6nw_state_t *pState)
{
    am_hal_xspimc_dev_cfg_t *pDevCfg = &pState->sDeviceCfgs.sDevCfg;

    memset(pDevCfg, 0, sizeof(am_hal_xspimc_dev_cfg_t));

    *pDevCfg = sDevCfgW958D6NWDefault;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set default AIP configuration.
//!
//! @param pState is the W958D6NW device state.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_default_aipcfg_set(w958d6nw_state_t *pState)
{
    am_hal_xspimc_aip_cfg_t *pAIPCfg = &pState->sDeviceCfgs.sAIPCfg;

    memset(pAIPCfg, 0, sizeof(am_hal_xspimc_aip_cfg_t));

    pAIPCfg->bAIPEnable   = false;
    pAIPCfg->ui32PageSize = AM_DEVICES_XSPI_PSRAM_W958D6NW_PAGE_SIZE;
    pAIPCfg->ePermission  = AM_HAL_XSPIMC_AIP_RW;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set default transfer configuration.
//!
//! @param pState is the W958D6NW device state.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_default_xfer_set(w958d6nw_state_t *pState)
{
    am_hal_xspimc_xfer_t *pXfer = &pState->sXfer;

    memset(pXfer, 0, sizeof(am_hal_xspimc_xfer_t));

    pXfer->eXferMode   = AM_HAL_XSPIMC_PIO_XFER;
    pXfer->ui8TxDummy  = sDevCfgW958D6NWDefault.ui8TxDummy;
    pXfer->ui8RxDummy  = sDevCfgW958D6NWDefault.ui8RxDummy;
    pXfer->ui8CmdLen   = sDevCfgW958D6NWDefault.ui8CmdLen;
    pXfer->ui8AddrLen  = sDevCfgW958D6NWDefault.ui8AddrLen;
    pXfer->ui32Timeout = AM_DEVICES_XSPI_PSRAM_TIMEOUT;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set default timing configuration.
//!
//! @param pState is the W958D6NW device state.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_default_timing_set(w958d6nw_state_t *pState)
{
    am_hal_xspimc_timing_cfg_t *pTimingCfg = &pState->sDeviceCfgs.sTimingCfg;

    memset(pTimingCfg, 0, sizeof(am_hal_xspimc_timing_cfg_t));

    pTimingCfg->ui8DataDelay = 0x00;
    pTimingCfg->ui8DQSDelay  = 0x9;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set all default state.
//!
//! @param pState is the W958D6NW device state.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_default_set(w958d6nw_state_t *pState)
{
    w958d6nw_default_regs_set(pState);

    if (w958d6nw_default_devcfg_set(pState))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if (w958d6nw_default_aipcfg_set(pState))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if (w958d6nw_default_xfer_set(pState))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if (w958d6nw_default_timing_set(pState))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Generic command write function.
//!
//! @param pState is the W958D6NW device state.
//! @param ui16Instr is the command instruction.
//! @param ui8AddrLen is the address length in bytes.
//! @param ui32Addr is the command address.
//! @param pData is the data buffer to write.
//! @param ui32NumBytes is the number of bytes to write.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_command_write(w958d6nw_state_t *pState,
                       uint16_t ui16Instr,
                       uint8_t ui8AddrLen,
                       uint32_t ui32Addr,
                       uint8_t *pData,
                       uint32_t ui32NumBytes)
{
    am_hal_xspimc_xfer_packet_t sPacket;

    pState->sXfer.eXferMode     = AM_HAL_XSPIMC_PIO_XFER;
    pState->sXfer.ui8TxDummy    = 0;
    pState->sXfer.ui8CmdLen     = 2;
    pState->sXfer.ui8AddrLen    = ui8AddrLen;
    pState->sXfer.pPackets      = &sPacket;
    pState->sXfer.ui32NumPacket = 1;

    sPacket.eDir         = AM_HAL_XSPIMC_XFER_WRITE;
    sPacket.ui16Cmd      = ui16Instr;
    sPacket.ui32Address  = ui32Addr;
    sPacket.pDataBuff    = pData;
    sPacket.ui32NumBytes = ui32NumBytes;

    return am_hal_xspimc_xfer(pState->pBus, &pState->sDeviceID, &pState->sXfer);
}

//*****************************************************************************
//
//! @brief Generic command read function.
//!
//! @param pState is the W958D6NW device state.
//! @param ui16Instr is the command instruction.
//! @param ui8AddrLen is the address length in bytes.
//! @param ui32Addr is the command address.
//! @param pData is the data buffer for read data.
//! @param ui32NumBytes is the number of bytes to read.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_command_read(w958d6nw_state_t *pState,
                      uint16_t ui16Instr,
                      uint8_t ui8AddrLen,
                      uint32_t ui32Addr,
                      uint8_t *pData,
                      uint32_t ui32NumBytes)
{
    am_hal_xspimc_xfer_packet_t sPacket;

    pState->sXfer.eXferMode     = AM_HAL_XSPIMC_PIO_XFER;
    pState->sXfer.ui8RxDummy    = pState->sDeviceCfgs.sDevCfg.ui8RxDummy;
    pState->sXfer.ui8CmdLen     = 2;
    pState->sXfer.ui8AddrLen    = ui8AddrLen;
    pState->sXfer.pPackets      = &sPacket;
    pState->sXfer.ui32NumPacket = 1;

    sPacket.eDir         = AM_HAL_XSPIMC_XFER_READ;
    sPacket.ui16Cmd      = ui16Instr;
    sPacket.ui32Address  = ui32Addr;
    sPacket.pDataBuff    = pData;
    sPacket.ui32NumBytes = ui32NumBytes;

    return am_hal_xspimc_xfer(pState->pBus, &pState->sDeviceID, &pState->sXfer);
}

//*****************************************************************************
//
//! @brief Read a W958D6NW register.
//!
//! @param pState is the W958D6NW device state.
//! @param ui32RegAddr is the register address.
//! @param pui16Value is the returned register value.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_register_read(w958d6nw_state_t *pState,
                       uint32_t ui32RegAddr,
                       uint16_t *pui16Value)
{
    uint16_t ui16PIOBuffer = 0;

    if (AM_DEVICES_XSPI_STATUS_SUCCESS !=
        w958d6nw_command_read(pState,
                              AM_DEVICES_XSPI_PSRAM_W958D6NW_DDR_READ_REGISTER,
                              AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ADDR_LEN,
                              ui32RegAddr,
                              (uint8_t *)&ui16PIOBuffer,
                              2))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    *pui16Value = __REV16(ui16PIOBuffer);

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Write a W958D6NW register.
//!
//! @param pState is the W958D6NW device state.
//! @param ui32RegAddr is the register address.
//! @param ui16Value is the register value to write.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_register_write(w958d6nw_state_t *pState,
                        uint32_t ui32RegAddr,
                        uint16_t ui16Value)
{
    uint16_t ui16PIOBuffer = __REV16(ui16Value);

    return w958d6nw_command_write(pState,
                                  AM_DEVICES_XSPI_PSRAM_W958D6NW_DDR_WRITE_REGISTER,
                                  AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ADDR_LEN,
                                  ui32RegAddr,
                                  (uint8_t *)&ui16PIOBuffer,
                                  2);
}

//*****************************************************************************
//
//! @brief Read all W958D6NW registers into the software register image.
//!
//! @param pState is the W958D6NW device state.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_registers_read(w958d6nw_state_t *pState)
{
    if (w958d6nw_register_read(pState,
                               AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ID0_ADDR,
                               &pState->sRegisters.ID0))
    {
        am_util_debug_printf("Failed to read PSRAM Register ID0!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (w958d6nw_register_read(pState,
                               AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ID1_ADDR,
                               &pState->sRegisters.ID1))
    {
        am_util_debug_printf("Failed to read PSRAM Register ID1!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (w958d6nw_register_read(pState,
                               AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_CR0_ADDR,
                               &pState->sRegisters.CR0))
    {
        am_util_debug_printf("Failed to read PSRAM Register CR0!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (w958d6nw_register_read(pState,
                               AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_CR1_ADDR,
                               &pState->sRegisters.CR1))
    {
        am_util_debug_printf("Failed to read PSRAM Register CR1!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_util_debug_printf("PSRAM Register ID0 = 0x%04X\n", pState->sRegisters.ID0);
    am_util_debug_printf("PSRAM Register ID1 = 0x%04X\n", pState->sRegisters.ID1);
    am_util_debug_printf("PSRAM Register CR0 = 0x%04X\n", pState->sRegisters.CR0);
    am_util_debug_printf("PSRAM Register CR1 = 0x%04X\n", pState->sRegisters.CR1);

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Validate ID fields from the current software register image.
//!
//! @param pState is the W958D6NW device state.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_id_check(w958d6nw_state_t *pState)
{
    if (pState->sRegisters.ID0_b.MANUFACTURER !=
        AM_DEVICES_XSPI_PSRAM_W958D6NW_VENDOR_ID)
    {
        am_util_debug_printf("Fail to get correct PSRAM Vendor ID!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if (pState->sRegisters.ID0_b.ROW_BITS != AM_DEVICES_XSPI_PSRAM_W958D6NW_ROW_BITS ||
        pState->sRegisters.ID0_b.COL_BITS != AM_DEVICES_XSPI_PSRAM_W958D6NW_COL_BITS)
    {
        am_util_debug_printf("Fail to get correct PSRAM density fields!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (pState->sRegisters.ID1_b.DEVICE_TYPE !=
        AM_DEVICES_XSPI_PSRAM_W958D6NW_DEVICE_ID)
    {
        am_util_debug_printf("Fail to get correct PSRAM Device Type!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Update device information from the current ID register image.
//!
//! @param pState is the W958D6NW device state.
//
//*****************************************************************************
static void
w958d6nw_info_update(w958d6nw_state_t *pState)
{
    pState->sDeviceInfo.ui8VendorId = pState->sRegisters.ID0_b.MANUFACTURER;
    pState->sDeviceInfo.ui8DeviceId = pState->sRegisters.ID1_b.DEVICE_TYPE;
    pState->sDeviceInfo.ui32DeviceSizeKb =
        1UL << (((pState->sRegisters.ID0_b.ROW_BITS + 1) +
                 (pState->sRegisters.ID0_b.COL_BITS + 1) + 2) - 10);
}

//*****************************************************************************
//
//! @brief Clamp AIP size to the detected device density.
//!
//! @param pState is the W958D6NW device state.
//! @param ui32RequestedSize is the requested AIP window size.
//!
//! @return AIP size limited to the device density.
//
//*****************************************************************************
static uint32_t
w958d6nw_aip_size_clamp(w958d6nw_state_t *pState, uint32_t ui32RequestedSize)
{
    uint32_t ui32DeviceSize = pState->sDeviceInfo.ui32DeviceSizeKb * 1024u;

    if ((ui32DeviceSize != 0u) && (ui32RequestedSize > ui32DeviceSize))
    {
        ui32RequestedSize = ui32DeviceSize;
    }

    return ui32RequestedSize & ~0x3u;
}

//*****************************************************************************
//
//! @brief Configure CR0 for the target xSPI clock and DQS mode.
//!
//! @param pState is the W958D6NW device state.
//! @param pDevCfg is the target xSPIMC device configuration.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_configure_cr0(w958d6nw_state_t *pState,
                       am_hal_xspimc_dev_cfg_t *pDevCfg)
{
    uint8_t                                    ui8Latency;
    uint8_t                                    ui8DynLatency;
    uint16_t                                   ui16CR0Check;
    am_devices_xspi_psram_w958d6nw_regs_t      sRegsCheck;
    am_devices_xspi_psram_w958d6nw_ilc_e       eILC;

    if (w958d6nw_get_latency_from_freq(pDevCfg->ui32Freq, &eILC))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if (w958d6nw_get_latency_cycles(eILC, &ui8Latency))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    ui8DynLatency = ui8Latency * 2;

    pState->sRegisters.CR0_b.ILC = eILC;
    if (!pDevCfg->bDQS)
    {
        pState->sRegisters.CR0_b.FIXED_LATENCY = 1;
        ui8Latency *= 2;
        am_util_debug_printf("Set PSRAM Register CR0 for non-DQS mode\n");
    }
    else
    {
        pState->sRegisters.CR0_b.FIXED_LATENCY = 0;
        am_util_debug_printf("Set PSRAM Register CR0 for DQS mode\n");
    }

    if (w958d6nw_register_write(pState,
                                AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_CR0_ADDR,
                                pState->sRegisters.CR0))
    {
        am_util_debug_printf("Failed to write PSRAM Register CR0!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pDevCfg->ui8RxDummy    = ui8Latency;
    pDevCfg->ui8TxDummy    = ui8Latency;
    pDevCfg->ui8DynTxDummy = ui8DynLatency;

    pState->sDeviceCfgs.sDevCfg.ui8RxDummy    = ui8Latency;
    pState->sDeviceCfgs.sDevCfg.ui8TxDummy    = ui8Latency;
    pState->sDeviceCfgs.sDevCfg.ui8DynTxDummy = ui8DynLatency;

    if (w958d6nw_register_read(pState,
                               AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_CR0_ADDR,
                               &ui16CR0Check))
    {
        am_util_debug_printf("Failed to verify PSRAM Register CR0!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sRegsCheck.CR0 = ui16CR0Check;
    if (sRegsCheck.CR0_b.ILC != pState->sRegisters.CR0_b.ILC ||
        sRegsCheck.CR0_b.FIXED_LATENCY != pState->sRegisters.CR0_b.FIXED_LATENCY)
    {
        am_util_debug_printf("Config PSRAM CR0 failed!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_util_debug_printf("PSRAM Register CR0 = 0x%04X\n", ui16CR0Check);
    am_util_debug_printf("Set PSRAM Latency = %d\n", ui8Latency);

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Convert public PSRAM config to xSPIMC device config.
//!
//! @param pState is the W958D6NW device state.
//! @param pCfg is the public PSRAM config.
//! @param pDevCfg is the returned xSPIMC device config.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_cov_devcfg(w958d6nw_state_t *pState,
                    const am_devices_xspi_psram_cfg_t *pCfg,
                    am_hal_xspimc_dev_cfg_t *pDevCfg)
{
    if (pCfg->eIOMode != AM_HAL_XSPIMC_IO_8_8_16 ||
        pCfg->eDataRate != AM_HAL_XSPIMC_DATA_RATE_DUAL)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    *pDevCfg = pState->sDeviceCfgs.sDevCfg;

    pDevCfg->ui32Freq     = pCfg->ui32Freq;
    pDevCfg->eIOMode      = pCfg->eIOMode;
    pDevCfg->eDataRate    = pCfg->eDataRate;
    pDevCfg->bDQS         = pCfg->bDQS;
    pDevCfg->ui8AddrAlign = 4;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Convert public memory-map config to xSPIMC AIP config.
//!
//! @param pState is the W958D6NW device state.
//! @param pCfg is the public PSRAM config.
//! @param pAipCfg is the returned AIP config.
//
//*****************************************************************************
static void
w958d6nw_cov_aipcfg(w958d6nw_state_t *pState,
                    const am_devices_xspi_psram_cfg_t *pCfg,
                    am_hal_xspimc_aip_cfg_t *pAipCfg)
{
    (void)pState;

    pAipCfg->ui32AIPBaseAddr         = pCfg->sMemMap.ui32BaseAddr;
    pAipCfg->ui32MappedDevBaseOffset = pCfg->sMemMap.ui32MappedDevBaseOffset;
    pAipCfg->ui32AIPSize             = w958d6nw_aip_size_clamp(pState,
                                                               pCfg->sMemMap.ui32Size);
    pAipCfg->bAIPEnable              = pCfg->sMemMap.bEnable;
    pAipCfg->ui8WindowNum            = pCfg->sMemMap.ui8WindowNum;
    pAipCfg->ePermission             = pCfg->sMemMap.ePermission;
    pAipCfg->bPrefetch               = pCfg->sMemMap.bPrefetch;
    pAipCfg->ui32PageSize            = AM_DEVICES_XSPI_PSRAM_W958D6NW_PAGE_SIZE;
}

//*****************************************************************************
//
//! @brief Pulls the hardware reset GPIO pin
//!
//! @param pPsram is the device handle.
//!
//! @return Status code.
//
//*****************************************************************************
static void
w958d6nw_hard_reset(am_device_xspi_periph_handle_t *pPsram)
{
    w958d6nw_state_t       *pState = (w958d6nw_state_t *)pPsram->pData;
    am_hal_periph_handle_t *pController = pState->pBus;
    am_hal_gpio_pincfg_t    sResetPincfg;
    uint32_t                ui32GPIONum = 0xFFFFFFFF;

    am_bsp_xspi_reset_pincfg_get(pController->ui8ModuleNum, pPsram->ui8ModuleNum,
                                 &ui32GPIONum, &sResetPincfg);

    if (ui32GPIONum != 0xFFFFFFFF)
    {
        am_hal_gpio_output_set(ui32GPIONum);
        am_hal_gpio_pinconfig(ui32GPIONum, sResetPincfg);
        am_hal_delay_us(200);
        am_hal_gpio_output_clear(ui32GPIONum);
        am_hal_delay_us(100);
        am_hal_gpio_output_set(ui32GPIONum);
    }
}

//*****************************************************************************
//
//! @brief Initialize the W958D6NW device registers.
//!
//! @param pPsram is the device handle.
//! @param pDevCfg is the target xSPIMC device configuration.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_device_init(am_device_xspi_periph_handle_t *pPsram,
                     am_hal_xspimc_dev_cfg_t *pDevCfg)
{
    w958d6nw_state_t *pState = (w958d6nw_state_t *)pPsram->pData;

    am_hal_periph_delay(pPsram, AM_DEVICES_XSPI_PSRAM_W958D6NW_TVCS_US);

    if (w958d6nw_registers_read(pState))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (w958d6nw_id_check(pState))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    w958d6nw_info_update(pState);

    if (w958d6nw_configure_cr0(pState, pDevCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Acquire the xSPI bus and enable the device.
//!
//! @param pPsram is the device handle.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_device_acquire(am_device_xspi_periph_handle_t *pPsram)
{
    w958d6nw_state_t *pState = (w958d6nw_state_t *)pPsram->pData;

    if (AM_HAL_STATUS_SUCCESS != am_hal_periph_acquire(pPsram, AM_DEVICES_XSPI_PSRAM_TIMEOUT))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_periph_handle_t *pController = pState->pBus;

    am_device_xspi_cpu_vote(pController->ui8ModuleNum, true);

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_enable(pState->pBus, &pState->sDeviceID))
    {
        (void)am_hal_periph_release(pPsram);
        am_device_xspi_cpu_vote(pController->ui8ModuleNum, false);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Disable the device and release the xSPI bus.
//!
//! @param pPsram is the device handle.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_device_release(am_device_xspi_periph_handle_t *pPsram)
{
    w958d6nw_state_t *pState = (w958d6nw_state_t *)pPsram->pData;

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_disable(pState->pBus, &pState->sDeviceID))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_periph_handle_t *pController = pState->pBus;

    am_device_xspi_cpu_vote(pController->ui8ModuleNum, false);

    if (AM_HAL_STATUS_SUCCESS != am_hal_periph_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

#define w958d6nw_device_error_ret w958d6nw_device_release

#if AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE
//*****************************************************************************
//
//! @brief Configure automatic hybrid-sleep entry and exit commands.
//!
//! @param pState is the W958D6NW device state.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_auto_power_config(w958d6nw_state_t *pState)
{
    pState->ui8LpEntryData[0] = (uint8_t)(pState->sRegisters.CR1 | (1u << 5));
    pState->ui8LpEntryData[1] = 0u;
    pState->ui8LpExitData[0]  = 0u;
    pState->ui8LpExitData[1]  = 0u;

    am_hal_xspimc_lp_cfg_t sLpCfg =
    {
        .bEnable   = true,
        .ui32Freq  = XSPI_W958D6NW_BASE_FREQUENCY,
        .eIOMode   = AM_HAL_XSPIMC_IO_OCTAL,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_DUAL,
        .sCmdEntry =
        {
            .ui16Cmd      = AM_DEVICES_XSPI_PSRAM_W958D6NW_DDR_WRITE_REGISTER,
            .ui8CmdLen    = 2,
            .ui8AddrLen   = AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ADDR_LEN,
            .ui32Address  = AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_CR1_ADDR,
            .ui32NumBytes = sizeof(pState->ui8LpEntryData),
            .pDataBuff    = pState->ui8LpEntryData,
        },
        .sCmdExit =
        {
            .ui16Cmd      = 0,
            .ui8CmdLen    = 2,
            .ui8AddrLen   = AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ADDR_LEN,
            .ui32Address  = 0,
            .ui32NumBytes = sizeof(pState->ui8LpExitData),
            .pDataBuff    = pState->ui8LpExitData,
        },
        .ui32MinEntryTime = AM_DEVICES_XSPI_PSRAM_W958D6NW_THS_US,
        .ui32MinExitTime  = AM_DEVICES_XSPI_PSRAM_W958D6NW_TEXTHS_US,
    };

    return am_hal_xspimc_control(pState->pBus,
                                 &pState->sDeviceID,
                                 AM_HAL_XSPIMC_CONTROL_AUTO_POWER_CONFIG,
                                 &sLpCfg);
}
#endif //!AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE

//*****************************************************************************
//
//! @brief Configure the W958D6NW device.
//!
//! @param pPsram is the device handle.
//! @param pConfig is the public PSRAM configuration.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_config(am_device_xspi_periph_handle_t *pPsram,
                const am_devices_xspi_psram_cfg_t *pConfig)
{
    w958d6nw_state_t          *pState = (w958d6nw_state_t *)pPsram->pData;
    am_hal_xspimc_dev_cfg_t   sTargetDevCfg;

    if (!pConfig)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_util_debug_printf("\nStart W958D6NW PSRAM Initialization\n");

    if (w958d6nw_device_acquire(pPsram))
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_power_control(pState->pBus,
                                                             &pState->sDeviceID,
                                                             AM_HAL_XSPIMC_POWER_REQUEST_ON))
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_device_configure(pState->pBus,
                                                                &pState->sDeviceID,
                                                                &pState->sDeviceCfgs.sDevCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI device.\n");
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_TIMING_CONFIG,
                                                       &pState->sDeviceCfgs.sTimingCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI timing.\n");
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != w958d6nw_cov_devcfg(pState, pConfig, &sTargetDevCfg))
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    w958d6nw_hard_reset(pPsram);

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != w958d6nw_device_init(pPsram, &sTargetDevCfg))
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_device_configure(pState->pBus,
                                                                &pState->sDeviceID,
                                                                &sTargetDevCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI device.\n");
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState->sDeviceCfgs.sDevCfg = sTargetDevCfg;

    w958d6nw_cov_aipcfg(pState, pConfig, &pState->sDeviceCfgs.sAIPCfg);
    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                                       &pState->sDeviceCfgs.sAIPCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI AIP.\n");
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

#if AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE
    if (AM_HAL_STATUS_SUCCESS != w958d6nw_auto_power_config(pState))
    {
        am_util_debug_printf("Error - Failed to configure XSPI automatic power.\n");
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
#endif

    if (w958d6nw_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Return cached W958D6NW device information.
//!
//! @param pPsram is the device handle.
//! @param pStateInfo is the returned device information.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_info(am_device_xspi_periph_handle_t *pPsram,
              am_devices_xspi_info_t *pStateInfo)
{
    w958d6nw_state_t *pState = (w958d6nw_state_t *)pPsram->pData;

    if (!pStateInfo)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    *pStateInfo = pState->sDeviceInfo;
    pStateInfo->ui32BaseAddr = pState->sDeviceCfgs.sAIPCfg.ui32AIPBaseAddr;
    pStateInfo->sCE          = pState->sDeviceID;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

#if !AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE
//*****************************************************************************
//
//! @brief Enter Hybrid Sleep.
//!
//! @param pPsram is the device handle.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_enter_hybridsleep(am_device_xspi_periph_handle_t *pPsram)
{
    w958d6nw_state_t        *pState      = (w958d6nw_state_t *)pPsram->pData;
    am_hal_xspimc_dev_cfg_t  sTempCfg    = pState->sDeviceCfgs.sDevCfg;
    am_hal_xspimc_aip_cfg_t  sTempAipCfg = pState->sDeviceCfgs.sAIPCfg;

    sTempAipCfg.bAIPEnable = false;
    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                                       &sTempAipCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sTempCfg.eIOMode      = AM_HAL_XSPIMC_IO_OCTAL;
    sTempCfg.eDataRate    = AM_HAL_XSPIMC_DATA_RATE_DUAL;
    sTempCfg.ui8AddrAlign = 1;
    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_device_configure(pState->pBus,
                                                                &pState->sDeviceID,
                                                                &sTempCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (w958d6nw_register_read(pState,
                               AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_CR1_ADDR,
                               &pState->sRegisters.CR1))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState->sRegisters.CR1_b.HYBRID_SLEEP = 1;

    if (w958d6nw_register_write(pState,
                                AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_CR1_ADDR,
                                pState->sRegisters.CR1))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_periph_delay(pPsram, AM_DEVICES_XSPI_PSRAM_W958D6NW_THS_US);

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Exit Hybrid Sleep.
//!
//! @param pPsram is the device handle.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_exit_hybridsleep(am_device_xspi_periph_handle_t *pPsram)
{
    w958d6nw_state_t *pState = (w958d6nw_state_t *)pPsram->pData;
    uint16_t          ui16PIOBuffer = 0;

    if (AM_HAL_STATUS_SUCCESS != w958d6nw_command_write(pState,
                                                        0x0000,
                                                        AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ADDR_LEN,
                                                        0,
                                                        (uint8_t *)&ui16PIOBuffer,
                                                        2))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_periph_delay(pPsram, AM_DEVICES_XSPI_PSRAM_W958D6NW_TEXTHS_US);

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_device_configure(pState->pBus,
                                                                &pState->sDeviceID,
                                                                &pState->sDeviceCfgs.sDevCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                                       &pState->sDeviceCfgs.sAIPCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Control W958D6NW power state.
//!
//! @param pPsram is the device handle.
//! @param ePowerMode is the requested power mode.
//! @param pPowerCfg is the optional power configuration.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_power(am_device_xspi_periph_handle_t *pPsram,
               am_devices_xspi_power_e ePowerMode,
               const am_devices_xspi_psram_w958d6nw_power_cfg_t *pPowerCfg)
{
    w958d6nw_state_t *pState = (w958d6nw_state_t *)pPsram->pData;

    (void)pPowerCfg;

    if (w958d6nw_device_acquire(pPsram))
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    switch (ePowerMode)
    {
        case AM_DEVICES_XSPI_POWER_ON:
            if (am_hal_xspimc_power_control(pState->pBus, &pState->sDeviceID,
                                            AM_HAL_XSPIMC_POWER_REQUEST_ON))
            {
                w958d6nw_device_error_ret(pPsram);
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
            if (w958d6nw_exit_hybridsleep(pPsram))
            {
                w958d6nw_device_error_ret(pPsram);
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
            break;

        case AM_DEVICES_XSPI_POWER_OFF:
            if (w958d6nw_enter_hybridsleep(pPsram))
            {
                w958d6nw_device_error_ret(pPsram);
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
            if (am_hal_xspimc_power_control(pState->pBus, &pState->sDeviceID,
                                            AM_HAL_XSPIMC_POWER_REQUEST_OFF))
            {
                w958d6nw_device_error_ret(pPsram);
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
            break;

        case AM_DEVICES_XSPI_POWER_AUTO:
        default:
            w958d6nw_device_error_ret(pPsram);
            return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (w958d6nw_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

#endif //!AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE

//*****************************************************************************
//
//! @brief Configure W958D6NW AIP memory mapping.
//!
//! @param pPsram is the device handle.
//! @param pCfg is the memory map configuration.
//!
//! @return Status code.
//
//*****************************************************************************
static uint32_t
w958d6nw_mem_map_config(am_device_xspi_periph_handle_t *pPsram,
                        const am_devices_xspi_memmap_cfg_t *pCfg)
{
    w958d6nw_state_t *pState;

    if (!pCfg)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (w958d6nw_state_t *)pPsram->pData;

    if (w958d6nw_device_acquire(pPsram))
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_xspimc_aip_cfg_t *pAipCfg = &pState->sDeviceCfgs.sAIPCfg;

    if (pCfg->bEnable)
    {
        pAipCfg->ui32AIPBaseAddr         = pCfg->ui32BaseAddr;
        pAipCfg->ui32MappedDevBaseOffset = pCfg->ui32MappedDevBaseOffset;
        pAipCfg->ui32AIPSize             = w958d6nw_aip_size_clamp(pState,
                                                                   pCfg->ui32Size);
        pAipCfg->bPrefetch               = pCfg->bPrefetch;
    }
    pAipCfg->ui8WindowNum = pCfg->ui8WindowNum;
    pAipCfg->bAIPEnable   = pCfg->bEnable;
    pAipCfg->ePermission  = pCfg->ePermission;

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                                       pAipCfg))
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (pCfg->bEnable)
    {
        /*am_hal_xspimc_security_cfg_t *pSecCfg = &pState->sDeviceCfgs.sSecurityCfg;

        pSecCfg->ui32BaseAddr = pCfg->ui32EncryptBaseAddr;
        pSecCfg->ui32Size     = pCfg->ui32EncryptSize;
        pSecCfg->bEnable      = (pCfg->ui32EncryptSize != 0u);
        pSecCfg->key          = pCfg->ui32EncryptKey;

        if (AM_HAL_STATUS_SUCCESS !=
            am_hal_xspimc_control(pState->pBus, &pState->sDeviceID,
                                  AM_HAL_XSPIMC_CONTROL_SECURITY_CONFIG,
                                  pSecCfg))
        {
            w958d6nw_device_error_ret(pPsram);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }*/
    }

    if (w958d6nw_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Initialize the W958D6NW xSPI PSRAM driver.
//!
//! @param pPsram is the device handle.
//!
//! @return Status code.
//
//*****************************************************************************
uint32_t
am_devices_xspi_psram_w958d6nw_init(am_device_xspi_periph_handle_t *pPsram)
{
    w958d6nw_state_t *pState;
    uint32_t          ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    if (pPsram->pData == NULL ||
        pPsram->uintSize < sizeof(w958d6nw_state_t))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_hal_periph_preinit(pPsram);
    if (ui32Status)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (w958d6nw_state_t *)pPsram->pData;
    memset(pState, 0, sizeof(w958d6nw_state_t));

    pState->sDeviceID = (am_hal_xspimc_dev_id)
    {
        .ui8HWCENum = pPsram->ui8ModuleNum,
        .eCEMode    = AM_HAL_XSPIMC_HARDWARE_CE,
    };

    {
        const am_devices_xspi_deps_t *pDeps = (const am_devices_xspi_deps_t *)pPsram->pDeps;
        if (!pDeps || !pDeps->pXSPI)
        {
            (void)am_hal_periph_postdeinit(pPsram);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        pState->pBus = pDeps->pXSPI;
    }

    if (w958d6nw_default_set(pState))
    {
        (void)am_hal_periph_postdeinit(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pPsram->state.bInitialized = true;
    pPsram->state.ui8InitRet   = ui32Status;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Deinitialize the W958D6NW xSPI PSRAM driver.
//!
//! @param pPsram is the device handle.
//!
//! @return Status code.
//
//*****************************************************************************
uint32_t
am_devices_xspi_psram_w958d6nw_deinit(am_device_xspi_periph_handle_t *pPsram)
{
    uint32_t ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    if (w958d6nw_device_acquire(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    w958d6nw_state_t *pState = (w958d6nw_state_t *)pPsram->pData;

    ui32Status = am_hal_xspimc_power_control(pState->pBus,
                                             &pState->sDeviceID,
                                             AM_HAL_XSPIMC_POWER_REQUEST_OFF);
    if (ui32Status)
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (w958d6nw_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_hal_periph_postdeinit(pPsram);
    if (ui32Status)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pPsram->state.bInitialized = false;
    pPsram->state.ui8InitRet   = 0xFF;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Apply xSPIMC timing settings.
//!
//! @param pPsram is the device handle.
//! @param pCfg is the timing configuration.
//!
//! @return Status code.
//
//*****************************************************************************
uint32_t
am_devices_xspi_psram_w958d6nw_timing_apply(am_device_xspi_periph_handle_t *pPsram,
                                            const am_devices_xspi_timing_t *pCfg)
{
    w958d6nw_state_t *pState;

    if (!pCfg)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (w958d6nw_state_t *)pPsram->pData;

    if (w958d6nw_device_acquire(pPsram))
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_TIMING_CONFIG,
                                                       pCfg))
    {
        w958d6nw_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState->sDeviceCfgs.sTimingCfg = *pCfg;

    if (w958d6nw_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief W958D6NW xSPI device API table.
//
//*****************************************************************************
const am_device_xspi_func_t apiW958D6NW =
{
    .device_init           = am_devices_xspi_psram_w958d6nw_init,
    .device_deinit         = am_devices_xspi_psram_w958d6nw_deinit,
    .device_config         = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                           const am_devices_xspi_config_t *,
                                           ...)) w958d6nw_config,
    .device_get_info       = w958d6nw_info,
    .device_write          = NULL,
    .device_read           = NULL,
    .device_erase          = NULL,
    .device_set_xfer_mode  = NULL,
#if AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE
    .device_power_ctrl     = NULL,
#else
    .device_power_ctrl     = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                          am_devices_xspi_power_e,
                                          ...)) w958d6nw_power,
#endif
    .device_mem_map_config = w958d6nw_mem_map_config,
    .device_timing_apply   = am_devices_xspi_psram_w958d6nw_timing_apply,
};

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
