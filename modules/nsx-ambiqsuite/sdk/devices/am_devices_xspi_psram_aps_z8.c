//*****************************************************************************
//
//! @file am_devices_xspi_psram_aps_z8.c
//!
//! @brief Multi-bit SPI PSRAM driver for the APS25616N/APSZ8 device.
//!
//! @addtogroup devices_xspi_psram_aps_z8 APS25616N/APS25616B xSPI PSRAM Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a hardware abstraction layer
//!          for the APS Z8 Multi-bit SPI PSRAM devices. It enables
//!          low-voltage operation, high-speed read/write operations, DDR mode
//!          support, and XIP functionality for embedded applications requiring
//!          power-efficient external volatile memory. The driver supports efficient
//!          data access, timing optimization, and system integration.
//!
//! @section devices_xspi_psram_aps_z8_features Key Features
//!
//! 1. @b Low-voltage @b Operation: Optimized for 1.2V power supply.
//! 2. @b High-speed @b Access: Multi-bit SPI for fast read/write operations.
//! 3. @b DDR @b Support: Double Data Rate for maximum throughput.
//! 4. @b XIP @b Mode: Execute code directly from PSRAM.
//! 5. @b Power @b Management: Enhanced low-power modes and fast wake-up.
//!
//! @section devices_xspi_psram_aps_z8_functionality Functionality
//!
//! - Initialize and configure APSZ8 device
//! - Perform read/write operations with DMA support
//! - Handle DDR mode configuration
//! - Manage XIP mode settings
//! - Control power states and voltage modes
//!
//! @section devices_xspi_psram_aps_z8_usage Usage
//!
//! 1. Initialize device with am_devices_xspi_psram_aps_z8_init()
//! 2. Read/write data using blocking or non-blocking APIs
//! 3. Enable XIP mode for code execution
//! 4. Configure DDR mode as needed
//!
//! @section devices_xspi_psram_aps_z8_configuration Configuration
//!
//! - Set up XSPI interface and timing parameters
//! - Configure voltage and power settings
//! - Enable DDR mode features
//! - Optimize power management settings
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
#include "am_devices_xspi_psram_aps_z8.h"
//#include "am_bsp.h"
#include "am_util.h"

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************

#define XSPI_BASE_FREQUENCY 25000000

#define APSZ8_tHS_MIN_US        155   // with margin
#define APSZ8_tXHS_MIN_US       155 // with margin

#define AM_DEVICES_XSPI_PSRAM_TIMEOUT             1000000
#define PSRAM_TIMING_SCAN_MIN_ACCEPTANCE_LENGTH   (3)     // there should be at least
                                                          // this amount of consecutive
                                                          // passing settings to be accepted.

typedef struct
{
    void                                *pBus;
    am_devices_xspi_psram_aps_z8_regs_t sRegisters;        //Device registers
    am_devices_xspi_info_t              sDeviceInfo;       //Device Info
    am_devices_xspi_hal_config_t        sDeviceCfgs;       //Device configurations
    am_hal_xspimc_dev_id                sDeviceID;
    am_hal_xspimc_xfer_t                sXfer;
} aps_z8_state_t;

#if (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION < 6000000)
__asm void
APSZ8_tXPHS_delay( uint32_t ui32Iterations )
{
    subs r0, #1
    bne APSZ8_tXPHS_delay
    bx lr
}
#elif (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION >= 6000000)
void
APSZ8_tXPHS_delay( uint32_t ui32Iterations )
{
    __asm
    (
        " subs  r0, #1\n"
        " bne   APSZ8_tXPHS_delay\n"
    );
}
#elif defined(__GNUC_STDC_INLINE__)
__attribute__((naked))
void
APSZ8_tXPHS_delay( uint32_t ui32Iterations )
{
    __asm
    (
        "   subs    r0, #1\n"
        "   bne     APSZ8_tXPHS_delay\n"
        "   bx      lr\n"
    );
}
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma diag_suppress = Pe940   // Suppress IAR compiler warning about missing
                                // return statement on a non-void function
__stackless inline void
APSZ8_tXPHS_delay( uint32_t ui32Iterations )
{
    __asm(" subs    r0, #1 ");
    __asm(" bne     APSZ8_tXPHS_delay ");
}
#pragma diag_default = Pe940    // Restore IAR compiler warning
#else
#error Compiler is unknown, please contact Ambiq support team
#endif

static const am_hal_xspimc_dev_cfg_t sDevCfgAPSZ8Default =
{
    .ui32Freq     = XSPI_BASE_FREQUENCY,
    .eIOMode      = AM_HAL_XSPIMC_IO_OCTAL,
    .eDataRate    = AM_HAL_XSPIMC_DATA_RATE_S_D_D,
    .eCpp         = AM_HAL_XSPIMC_CPP_MODE_0,
    .bDQS         = true,
    .ui32ReadCmd  = AM_DEVICES_XSPI_PSRAM_APS_Z8_READ,
    .ui32WriteCmd = AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE,
    .ui8CmdLen    = 1,
    .ui8AddrLen   = 4,
    .ui8AddrAlign = 1,
    .sCETiming    =
    {
        .ui32TCEMaxAct    = 4000,
        .ui32TCESetupHold = 2,
        .ui32TCEMinInact  = 20,
    },
    .eDeviceType = AM_HAL_XSPIMC_PSRAM_DEVICE,
};

#if AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE
static const uint8_t ui8LpEntryData[2] = {0xF0u, 0x0u};
static const uint8_t ui8LpExitData[2]  = {0x0u, 0x0u};
static const am_hal_xspimc_lp_cfg_t g_sLpCfg =
{
    .bEnable   = true,
    .ui32Freq  = XSPI_BASE_FREQUENCY,
    .eIOMode   = AM_HAL_XSPIMC_IO_OCTAL,
    .eDataRate = AM_HAL_XSPIMC_DATA_RATE_S_D_D,
    .sCmdEntry =
    {
        .ui16Cmd      = AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE_REGISTER,
        .ui8CmdLen    = 1,
        .ui8AddrLen   = AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
        .ui32Address  = 6,
        .ui32NumBytes = sizeof(ui8LpEntryData),
        .pDataBuff    = (uint8_t *)ui8LpEntryData,
    },
    .sCmdExit =
    {
        .ui16Cmd      = 0,
        .ui8CmdLen    = 1,
        .ui8AddrLen   = AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
        .ui32Address  = 0,
        .ui32NumBytes = sizeof(ui8LpExitData),
        .pDataBuff    = (uint8_t *)ui8LpExitData,
    },
    .ui32MinEntryTime = APSZ8_tHS_MIN_US,
    .ui32MinExitTime  = APSZ8_tXHS_MIN_US,
};
#endif

static inline uint32_t aps_z8_get_txdummy(am_devices_xspi_psram_aps_z8_wlc_e eWLC, uint8_t *pWLC)
{
    switch(eWLC)
    {
#if !defined(USE_APS51216BA)
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_3:
            *pWLC = 3;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_4:
            *pWLC = 4;
            break;
#endif
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_5:
            *pWLC = 5;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_6:
            *pWLC = 6;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_7:
            *pWLC = 7;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_8:
            *pWLC = 8;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_9:
            *pWLC = 9;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_10:
            *pWLC = 10;
            break;
        default:
            return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static inline uint32_t aps_z8_get_rxdummy(am_devices_xspi_psram_aps_z8_rlc_e eRLC, uint8_t *pRLC)
{
    switch(eRLC)
    {
#if !defined(USE_APS51216BA)
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_3:
            *pRLC = 3;
            break;
#endif
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_4:
            *pRLC = 4;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_5:
            *pRLC = 5;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_6:
            *pRLC = 6;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_7:
            *pRLC = 7;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_8:
            *pRLC = 8;
            break;
        case AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_9:
            *pRLC = 9;
            break;
        default:
            return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static inline uint32_t aps_z8_get_wlc_from_freq(uint32_t ui32Freq,
                                                am_devices_xspi_psram_aps_z8_wlc_e *peWLC)
{
// #### INTERNAL BEGIN ####
#if defined(ATOMIQ11X_FPGA)
    *peWLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_3;
    return AM_DEVICES_XSPI_STATUS_SUCCESS;
#endif
// #### INTERNAL END ####

    if (ui32Freq <= 66000000)
    {
        *peWLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_3;
    }
    else if (ui32Freq > 66000000 && ui32Freq <= 109000000)
    {
        *peWLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_4;
    }
    else if (ui32Freq > 109000000 && ui32Freq <= 133000000)
    {
        *peWLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_5;
    }
    else if (ui32Freq > 133000000 && ui32Freq <= 166000000)
    {
        *peWLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_6;
    }
    else if (ui32Freq > 166000000 && ui32Freq <= 200000000)
    {
        *peWLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_7;
    }
    else
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static inline uint32_t aps_z8_get_rlc_from_freq(uint32_t ui32Freq,
                                                am_devices_xspi_psram_aps_z8_rlc_e *peRLC)
{
// #### INTERNAL BEGIN ####
#if defined(ATOMIQ11X_FPGA)
    *peRLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_3;
    return AM_DEVICES_XSPI_STATUS_SUCCESS;
#endif
// #### INTERNAL END ####

    if (ui32Freq <= 66000000)
    {
        *peRLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_3;
    }
    else if (ui32Freq > 66000000 && ui32Freq <= 109000000)
    {
        *peRLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_4;
    }
    else if (ui32Freq > 109000000 && ui32Freq <= 133000000)
    {
        *peRLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_5;
    }
    else if (ui32Freq > 133000000 && ui32Freq <= 166000000)
    {
        *peRLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_6;
    }
    else if (ui32Freq > 166000000 && ui32Freq <= 200000000)
    {
        *peRLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_7;
    }
    else
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static void
aps_z8_default_regs_set(aps_z8_state_t *pState)
{
    memset(&pState->sRegisters, 0, sizeof(pState->sRegisters));

    pState->sRegisters.MR0_b.DS  = AM_DEVICES_XSPI_PSRAM_APS_Z8_DRIVE_STRENGTH_FULL;
    pState->sRegisters.MR0_b.RLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_5;
    pState->sRegisters.MR4_b.WLC = AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_5;

    pState->sRegisters.MR8_b.BL = 1;
    pState->sRegisters.MR8_b.BT = 1;
}

static uint32_t
aps_z8_default_devcfg_set(aps_z8_state_t *pState)
{
    am_hal_xspimc_dev_cfg_t *pDevcfg = &pState->sDeviceCfgs.sDevCfg;

    memset(pDevcfg, 0, sizeof(am_hal_xspimc_dev_cfg_t));

    pDevcfg->ui32Freq     = sDevCfgAPSZ8Default.ui32Freq;
    pDevcfg->eIOMode      = sDevCfgAPSZ8Default.eIOMode;
    pDevcfg->eDataRate    = sDevCfgAPSZ8Default.eDataRate;
    pDevcfg->eCpp         = sDevCfgAPSZ8Default.eCpp;
    pDevcfg->bDQS         = sDevCfgAPSZ8Default.bDQS;
    pDevcfg->ui32ReadCmd  = sDevCfgAPSZ8Default.ui32ReadCmd;
    pDevcfg->ui32WriteCmd = sDevCfgAPSZ8Default.ui32WriteCmd;
    pDevcfg->ui8CmdLen    = sDevCfgAPSZ8Default.ui8CmdLen;
    pDevcfg->ui8AddrLen   = sDevCfgAPSZ8Default.ui8AddrLen;
    pDevcfg->ui8AddrAlign = sDevCfgAPSZ8Default.ui8AddrAlign;
    if (aps_z8_get_txdummy(pState->sRegisters.MR4_b.WLC, &pDevcfg->ui8TxDummy))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if (aps_z8_get_rxdummy(pState->sRegisters.MR0_b.RLC, &pDevcfg->ui8RxDummy))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pDevcfg->sCETiming       = sDevCfgAPSZ8Default.sCETiming;
    pDevcfg->ui32MemBoundary = AM_DEVICES_XSPI_PSRAM_APS_Z8_PAGE_SIZE;
    pDevcfg->eDeviceType     = sDevCfgAPSZ8Default.eDeviceType;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
aps_z8_default_aipcfg_set(aps_z8_state_t *pState)
{
    am_hal_xspimc_aip_cfg_t *pAIPCfg = &pState->sDeviceCfgs.sAIPCfg;

    memset(pAIPCfg, 0, sizeof(am_hal_xspimc_aip_cfg_t));

    pAIPCfg->bAIPEnable   = false;
    pAIPCfg->ui32PageSize = 1024;
    pAIPCfg->ePermission  = AM_HAL_XSPIMC_AIP_RW;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
aps_z8_default_xfer_set(aps_z8_state_t *pState)
{
    am_hal_xspimc_xfer_t *pXfer = &pState->sXfer;

    memset(pXfer, 0, sizeof(am_hal_xspimc_xfer_t));

    pXfer->eXferMode = AM_HAL_XSPIMC_PIO_XFER;
    if (aps_z8_get_txdummy(pState->sRegisters.MR4_b.WLC, &pXfer->ui8TxDummy))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if (aps_z8_get_rxdummy(pState->sRegisters.MR0_b.RLC, &pXfer->ui8RxDummy))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    pXfer->ui8CmdLen   = 1;
    pXfer->ui8AddrLen  = 4;
    pXfer->ui32Timeout = AM_DEVICES_XSPI_PSRAM_TIMEOUT;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
aps_z8_default_timing_set(aps_z8_state_t *pState)
{
    am_hal_xspimc_timing_cfg_t *pTimingCfg = &pState->sDeviceCfgs.sTimingCfg;

    memset(pTimingCfg, 0, sizeof(am_hal_xspimc_timing_cfg_t));

    pTimingCfg->ui8DataDelay = 0x00;
    pTimingCfg->ui8DQSDelay  = 0x9;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
aps_z8_default_set(aps_z8_state_t *pState)
{
    aps_z8_default_regs_set(pState);
    if ( aps_z8_default_devcfg_set(pState) )
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if ( aps_z8_default_aipcfg_set(pState) )
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if ( aps_z8_default_xfer_set(pState) )
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if ( aps_z8_default_timing_set(pState) )
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}


//*****************************************************************************
//
//! @brief Generic Command Write function.
//!
//! @param pBus
//! @param ui16Instr
//! @param bSendAddr
//! @param ui32Addr
//! @param pData
//! @param ui32NumBytes
//!
//! @return
//
//*****************************************************************************
static uint32_t
am_device_command_write(aps_z8_state_t *pState,
                        uint16_t ui16Instr,
                        uint8_t ui8AddrLen,
                        uint32_t ui32Addr,
                        uint8_t *pData,
                        uint32_t ui32NumBytes)
{
    am_hal_xspimc_xfer_packet_t sPacket;

    pState->sXfer.eXferMode     = AM_HAL_XSPIMC_PIO_XFER;
    pState->sXfer.ui8TxDummy    = 0;
    pState->sXfer.ui8CmdLen     = 1;
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
//! @brief Generic Command Read function.
//!
//! @param pBus
//! @param ui16Instr
//! @param bSendAddr
//! @param ui32Addr
//! @param pData
//! @param ui32NumBytes
//!
//! @return
//
//*****************************************************************************
static uint32_t
am_device_command_read(aps_z8_state_t *pState,
                       uint16_t ui16Instr,
                       uint8_t ui8AddrLen,
                       uint32_t ui32Addr,
                       uint8_t *pData,
                       uint32_t ui32NumBytes)
{
    am_hal_xspimc_xfer_packet_t sPacket;

    pState->sXfer.eXferMode     = AM_HAL_XSPIMC_PIO_XFER;
    pState->sXfer.ui8RxDummy    = pState->sDeviceCfgs.sDevCfg.ui8RxDummy;
    pState->sXfer.ui8CmdLen     = 1;
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
//! @brief Reset the external psram
//!
//! @param pState
//!
//! @return
//
//*****************************************************************************
static uint32_t
aps_z8_reset(aps_z8_state_t *pState)
{
    uint16_t ui16PIOBuffer = 0;
    //
    // Global Reset DDR PSRAM.
    //
    if (AM_HAL_STATUS_SUCCESS != am_device_command_write(pState,
                                                         AM_DEVICES_XSPI_PSRAM_APS_Z8_GLOBAL_RESET,
                                                         0,
                                                         0,
                                                         (uint8_t *)&ui16PIOBuffer,
                                                         2))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
aps_z8_get_info(aps_z8_state_t *pState)
{
    uint16_t                            ui16PIOBuffer;
    am_devices_xspi_psram_aps_z8_regs_t sRegs = {0};

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != am_device_command_read(pState,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                                                 0,
                                                                 (uint8_t *)&ui16PIOBuffer,
                                                                 2))
    {
        am_util_debug_printf("Failed to read PSRAM MR0 Register!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    sRegs.MR0 = ui16PIOBuffer & 0xFF;
    am_util_debug_printf("PSRAM Register MR0 = 0x%X\n", sRegs.MR0);

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != am_device_command_read(pState,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                                                 1,
                                                                 (uint8_t *)&ui16PIOBuffer,
                                                                 2))
    {
        am_util_debug_printf("Failed to read PSRAM MR1 Register!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    sRegs.MR1 = ui16PIOBuffer & 0xFF;
    am_util_debug_printf("PSRAM Register MR1 = 0x%X\n", sRegs.MR1);

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != am_device_command_read(pState,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                                                 2,
                                                                 (uint8_t *)&ui16PIOBuffer,
                                                                 2))
    {
        am_util_debug_printf("Failed to read PSRAM MR2 Register!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    sRegs.MR2 = ui16PIOBuffer & 0xFF;
    am_util_debug_printf("PSRAM Register MR2 = 0x%X\n", sRegs.MR2);

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != am_device_command_read(pState,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                                                 3,
                                                                 (uint8_t *)&ui16PIOBuffer,
                                                                 2))
    {
        am_util_debug_printf("Failed to read PSRAM MR3 Register!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    sRegs.MR3 = ui16PIOBuffer & 0xFF;
    am_util_debug_printf("PSRAM Register MR3 = 0x%X\n", sRegs.MR3);

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != am_device_command_read(pState,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                                                 4,
                                                                 (uint8_t *)&ui16PIOBuffer,
                                                                 2))
    {
        am_util_debug_printf("Failed to read PSRAM MR4 Register!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    sRegs.MR4 = ui16PIOBuffer & 0xFF;
    am_util_debug_printf("PSRAM Register MR4 = 0x%X\n", sRegs.MR4);

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != am_device_command_read(pState,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                                                 6,
                                                                 (uint8_t *)&ui16PIOBuffer,
                                                                 2))
    {
        am_util_debug_printf("Failed to read PSRAM MR6 Register!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    sRegs.MR6 = ui16PIOBuffer & 0xFF;
    am_util_debug_printf("PSRAM Register MR6 = 0x%X\n", sRegs.MR6);

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != am_device_command_read(pState,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                                                 AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                                                 8,
                                                                 (uint8_t *)&ui16PIOBuffer,
                                                                 2))
    {
        am_util_debug_printf("Failed to read PSRAM MR8 Register!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    sRegs.MR8 = ui16PIOBuffer & 0xFF;
    am_util_debug_printf("PSRAM Register MR8 = 0x%X\n", sRegs.MR8);

    if (pState->sRegisters.MR0 != sRegs.MR0)
    {
        am_util_debug_printf("Register MR0 does not match default!!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (pState->sRegisters.MR4 != sRegs.MR4)
    {
        am_util_debug_printf("Register MR4 does not match default!!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (pState->sRegisters.MR8 != sRegs.MR8)
    {
        am_util_debug_printf("Register MR8 does not match default!!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    memcpy(&pState->sRegisters, &sRegs, sizeof(am_devices_xspi_psram_aps_z8_regs_t));

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set PSRAM into HEX mode
//!
//! @param pState
//!
//! @return
//
//*****************************************************************************
static uint32_t
aps_z8_enter_hex_mode(aps_z8_state_t *pState)
{
    uint32_t ui32Status;
    uint16_t ui16PIOBuffer = 0;
    //
    // Read PSRAM MR8 register
    //
    am_util_debug_printf("PSRAM IO mode = 0x%X\n\n", pState->sRegisters.MR8_b.IOM);

    pState->sRegisters.MR8_b.IOM = 1;
    ui16PIOBuffer                = pState->sRegisters.MR8;

    ui32Status = am_device_command_write(pState,
                                         AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE_REGISTER,
                                         AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                         8,
                                         (uint8_t *)&ui16PIOBuffer,
                                         2);
    if (AM_DEVICES_XSPI_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("Failed to set PSRAM into HEX mode!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    else
    {
        am_util_debug_printf("Set PSRAM into HEX mode\n\n");
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set PSRAM into Octal mode and exit HEX mode
//!
//! @param pBus
//!
//! @return
//
//*****************************************************************************
static uint32_t
aps_z8_exit_hex_mode(aps_z8_state_t *pState)
{
    uint32_t ui32Status;
    uint16_t ui16PIOBuffer = 0;

    pState->sRegisters.MR8_b.IOM = 0;
    ui16PIOBuffer                = pState->sRegisters.MR8;

    ui32Status = am_device_command_write(pState,
                                         AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE_REGISTER,
                                         AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                         8,
                                         (uint8_t *)&ui16PIOBuffer,
                                         2);
    if (AM_DEVICES_XSPI_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("Failed to configure PSRAM to exit HEX mode!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    else
    {
        am_util_debug_printf("PSRAM is set to Octal mode\n\n");
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Reads the ID of the external psram and returns the value.
//!
//! @param pState
//!
//! This function reads the device ID register of the external psram, and returns
//! the result as an 32-bit unsigned integer value.
//!
//! @return 32-bit status
//
//*****************************************************************************
static uint32_t
aps_z8_get_id(aps_z8_state_t *pState)
{
    am_util_debug_printf("\nPSRAM Register MR1 = 0x%X\n", pState->sRegisters.MR1);
    if ( pState->sRegisters.MR1_b.VID == 0xD )
    {
        am_util_debug_printf("PSRAM Vendor ID =  01101\n\n");
    }
    else
    {
        am_util_debug_printf("Fail to get correct PSRAM Vendor ID!\n\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_util_debug_printf("PSRAM Register MR2 = 0x%X\n", pState->sRegisters.MR2);
    am_util_debug_printf("PSRAM Device ID =  Generation %d\n",
                         pState->sRegisters.MR2_b.GENERATION ==
                         0 ? 5 : pState->sRegisters.MR2_b.GENERATION + 1);
    if ( pState->sRegisters.MR2_b.DENSITY == 0x1 )
    {
        am_util_debug_printf("PSRAM Density =  32Mb\n\n");
    }
    else if ( pState->sRegisters.MR2_b.DENSITY == 0x3 )
    {
        am_util_debug_printf("PSRAM Density =  64Mb\n\n");
    }
    else if ( pState->sRegisters.MR2_b.DENSITY == 0x5 )
    {
        am_util_debug_printf("PSRAM Density =  128Mb\n\n");
    }
    else if ( pState->sRegisters.MR2_b.DENSITY == 0x7 )
    {
        am_util_debug_printf("PSRAM Density =  256Mb\n\n");
    }
    else if ( pState->sRegisters.MR2_b.DENSITY == 0x6 )
    {
        am_util_debug_printf("PSRAM Density =  512Mb\n\n");
    }
    am_util_debug_printf("PSRAM Register MR3 = 0x%X\n", pState->sRegisters.MR3);
    if ( pState->sRegisters.MR3_b.RBXen )
    {
        am_util_debug_printf("PSRAM RBX Supported\n\n");
    }
    else
    {
        am_util_debug_printf("PSRAM RBX not Supported\n\n");
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static void
aps_z8_cov_devcfg(aps_z8_state_t *pState,
                  const am_devices_xspi_psram_cfg_t *pCfg,
                  am_hal_xspimc_dev_cfg_t *pDevCfg)
{
    *pDevCfg = pState->sDeviceCfgs.sDevCfg;

    pDevCfg->ui32Freq     = pCfg->ui32Freq;
    pDevCfg->eIOMode      = pCfg->eIOMode;
    pDevCfg->eDataRate    = pCfg->eDataRate;
    pDevCfg->bDQS         = pCfg->bDQS;
    pDevCfg->eDeviceType  = pCfg->eIOMode == AM_HAL_XSPIMC_IO_8_8_16 ?
                            AM_HAL_XSPIMC_APM_HEX_PSRAM_DEVICE:
                            AM_HAL_XSPIMC_PSRAM_DEVICE;
    pDevCfg->ui8AddrAlign = pCfg->eIOMode == AM_HAL_XSPIMC_IO_8_8_16 ?
                            2:1;
}

//*****************************************************************************
//
//! @brief Return the detected APS Z8 device density in bytes.
//!
//! @param pState is the APS Z8 device state.
//!
//! @return Device density in bytes, or 0 if the density field is unknown.
//
//*****************************************************************************
static uint32_t
aps_z8_device_size_get(aps_z8_state_t *pState)
{
    switch (pState->sRegisters.MR2_b.DENSITY)
    {
        case 1:
            return 32 / 8 * 1024U * 1024u;
        case 3:
            return 64 / 8 * 1024U * 1024u;
        case 5:
            return 128 / 8 * 1024U * 1024u;
        case 7:
            return 256 / 8 * 1024U * 1024u;
        case 6:
            return 512 / 8 * 1024U * 1024u;
        default:
            return 0u;
    }
}

//*****************************************************************************
//
//! @brief Clamp AIP size to the detected device density.
//!
//! @param pState is the APS Z8 device state.
//! @param ui32RequestedSize is the requested AIP window size.
//!
//! @return AIP size limited to the device density.
//
//*****************************************************************************
static uint32_t
aps_z8_aip_size_clamp(aps_z8_state_t *pState, uint32_t ui32RequestedSize)
{
    uint32_t ui32DeviceSize = aps_z8_device_size_get(pState);

    if ((ui32DeviceSize != 0u) && (ui32RequestedSize > ui32DeviceSize))
    {
        ui32RequestedSize = ui32DeviceSize;
    }

    return ui32RequestedSize & ~0x3u;
}

static void
aps_z8_cov_aipcfg(aps_z8_state_t *pState,
                  const am_devices_xspi_psram_cfg_t *pCfg,
                  am_hal_xspimc_aip_cfg_t *pAipCfg)
{
    pAipCfg->ui32AIPBaseAddr         = pCfg->sMemMap.ui32BaseAddr;
    pAipCfg->ui32MappedDevBaseOffset = pCfg->sMemMap.ui32MappedDevBaseOffset;
    pAipCfg->ui32AIPSize             = aps_z8_aip_size_clamp(pState,
                                                             pCfg->sMemMap.ui32Size);
    pAipCfg->bAIPEnable              = pCfg->sMemMap.bEnable;
    pAipCfg->ui8WindowNum            = pCfg->sMemMap.ui8WindowNum;
    pAipCfg->bPrefetch               = pCfg->sMemMap.bPrefetch;
}

static uint32_t
aps_z8_device_init(am_device_xspi_periph_handle_t *pPsram, am_hal_xspimc_dev_cfg_t *pDevCfg)
{
    aps_z8_state_t *pState       = (aps_z8_state_t *)pPsram->pData;
    uint32_t       ui32Status    = AM_DEVICES_XSPI_STATUS_SUCCESS;
    uint16_t       ui16PIOBuffer = 0;

    //
    // Send reset command to PSRAM
    //
    if (aps_z8_reset(pState))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_periph_delay(pPsram, 2);

    if (!pDevCfg->bDQS)
    {
        pState->sRegisters.MR0_b.LT = 1;
        ui16PIOBuffer               = pState->sRegisters.MR0;

        ui32Status = am_device_command_write(pState,
                                             AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE_REGISTER,
                                             AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                             0,
                                             (uint8_t *)&ui16PIOBuffer,
                                             2);
        if (ui32Status)
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    if (aps_z8_get_info(pState))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    //
    // Read PSRAM ID
    //
    if (aps_z8_get_id(pState))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_devices_xspi_psram_aps_z8_wlc_e eWLC;
    if (aps_z8_get_wlc_from_freq(pDevCfg->ui32Freq, &eWLC))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    pState->sRegisters.MR4_b.WLC = eWLC;

    if (aps_z8_get_txdummy(eWLC, &pDevCfg->ui8TxDummy))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_devices_xspi_psram_aps_z8_rlc_e eRLC;
    if (aps_z8_get_rlc_from_freq(pDevCfg->ui32Freq, &eRLC))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    pState->sRegisters.MR0_b.RLC = eRLC;

    if (aps_z8_get_rxdummy(eRLC, &pDevCfg->ui8RxDummy))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if ( !pDevCfg->bDQS )
    {
        pState->sRegisters.MR0_b.LT = 1;
        am_util_debug_printf("\nSet read latency into fixed type in NON-DQS mode!\n");
    }

    ui16PIOBuffer = pState->sRegisters.MR0;
    ui32Status    = am_device_command_write(pState,
                                            AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE_REGISTER,
                                            AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                            0,
                                            (uint8_t *)&ui16PIOBuffer,
                                            2);
    if (AM_DEVICES_XSPI_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("\nFailed to write PSRAM Register MR0!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    else
    {
        am_util_debug_printf("\nSet PSRAM Register MR0 into 0x%02X\n", pState->sRegisters.MR0);
        am_util_debug_printf("\nSet PSRAM Register Read Latency = %d\n", pDevCfg->ui8RxDummy);
    }

    ui16PIOBuffer = pState->sRegisters.MR4;
    ui32Status    = am_device_command_write(pState,
                                            AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE_REGISTER,
                                            AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                            4,
                                            (uint8_t *)&ui16PIOBuffer,
                                            2);
    if (AM_DEVICES_XSPI_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("Failed to write PSRAM Write Register MR4!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    else
    {
        am_util_debug_printf("Set PSRAM Register MR4 into 0x%02X\n\n", pState->sRegisters.MR4);
        am_util_debug_printf("\nSet PSRAM Register Write Latency = %d\n", pDevCfg->ui8TxDummy);
    }

    pState->sDeviceCfgs.sDevCfg.ui8TxDummy = pDevCfg->ui8TxDummy;
    pState->sDeviceCfgs.sDevCfg.ui8RxDummy = pDevCfg->ui8RxDummy;
    pState->sDeviceCfgs.sDevCfg.bDQS       = pDevCfg->bDQS;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
aps_z8_device_validate(am_device_xspi_periph_handle_t *pPsram)
{
    aps_z8_state_t *pState    = (aps_z8_state_t *)pPsram->pData;
    uint32_t       ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;
    uint16_t       u16PIOdata;

    am_util_debug_printf("Read PSRAM Read Latency Code\n");
    ui32Status = am_device_command_read(pState, AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                        AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN, 0,
                                        (uint8_t *)&u16PIOdata, 2);
    if (AM_DEVICES_XSPI_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("Failed to read PSRAM Read Latency Code!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    else
    {
        if ( (u16PIOdata & 0xFF) == pState->sRegisters.MR0 )
        {
            am_util_debug_printf("PSRAM Register MR0 = 0x%02X\nSet Success\n", (uint8_t)u16PIOdata);
        }
        else
        {
            am_util_debug_printf("PSRAM Register MR0 = 0x%02X\nSet Fail\n\n", (uint8_t)u16PIOdata);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    am_util_debug_printf("Read PSRAM Write Latency Code\n");
    ui32Status = am_device_command_read(pState, AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                        AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN, 4,
                                        (uint8_t *)&u16PIOdata, 2);
    if (AM_DEVICES_XSPI_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("Failed to read PSRAM Write Latency Code!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    else
    {
        if ( (u16PIOdata & 0xFF) == pState->sRegisters.MR4 )
        {
            am_util_debug_printf("PSRAM Register MR4 = 0x%02X\nSet Success\n", (uint8_t)u16PIOdata);
        }
        else
        {
            am_util_debug_printf("PSRAM Register MR4 = 0x%02X\nSet Fail\n\n", (uint8_t)u16PIOdata);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    am_util_debug_printf("Read PSRAM IO Mode\n");
    ui32Status = am_device_command_read(pState, AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER,
                                        AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN, 8,
                                        (uint8_t *)&u16PIOdata, 2);
    if (AM_DEVICES_XSPI_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("Failed to read PSRAM IO Mode!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    else
    {
        if ( (u16PIOdata & 0xFF) == pState->sRegisters.MR8 )
        {
            am_util_debug_printf("PSRAM Register MR8 = 0x%02X\nSet Success\n", (uint8_t)u16PIOdata);
        }
        else
        {
            am_util_debug_printf("PSRAM Register MR8 = 0x%02X\nSet Fail\n\n", (uint8_t)u16PIOdata);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
aps_z8_device_acquire(am_device_xspi_periph_handle_t *pPsram)
{
    aps_z8_state_t *pState = (aps_z8_state_t *)pPsram->pData;

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

static uint32_t
aps_z8_device_release(am_device_xspi_periph_handle_t *pPsram)
{
    aps_z8_state_t *pState = (aps_z8_state_t *)pPsram->pData;

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

#define aps_z8_device_error_ret aps_z8_device_release

static uint32_t
aps_z8_config(am_device_xspi_periph_handle_t *pPsram,
              const am_devices_xspi_psram_cfg_t *pConfig)
{
    aps_z8_state_t          *pState = (aps_z8_state_t *)pPsram->pData;
    am_hal_xspimc_dev_cfg_t sTargetDevCfg;

    am_util_debug_printf("\nStart PSRAM Initialization\n");

    if (aps_z8_device_acquire(pPsram))
    {
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_power_control(pState->pBus,
                                                             &pState->sDeviceID,
                                                             AM_HAL_XSPIMC_POWER_REQUEST_ON))
    {
        am_util_debug_printf("Error - Failed to bring up XSPI.\n");
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState->sDeviceCfgs.sDevCfg.bDQS = pConfig->bDQS;

    //
    // Initialize the xSPI controller with device default settings
    //
    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_device_configure(pState->pBus,
                                                                &pState->sDeviceID,
                                                                &pState->sDeviceCfgs.sDevCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI device.\n");
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_TIMING_CONFIG,
                                                       &pState->sDeviceCfgs.sTimingCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI device.\n");
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    aps_z8_cov_devcfg(pState, pConfig, &sTargetDevCfg);

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != aps_z8_device_init(pPsram, &sTargetDevCfg))
    {
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_device_configure(pState->pBus,
                                                                &pState->sDeviceID,
                                                                &pState->sDeviceCfgs.sDevCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI device.\n");
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (sTargetDevCfg.eIOMode == AM_HAL_XSPIMC_IO_8_8_16)
    {
        //
        // Set PSRAM into HEX mode
        //
        if (AM_HAL_STATUS_SUCCESS != aps_z8_enter_hex_mode(pState))
        {
            aps_z8_device_error_ret(pPsram);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    if (AM_DEVICES_XSPI_STATUS_SUCCESS != aps_z8_device_validate(pPsram))
    {
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState->sDeviceCfgs.sDevCfg = sTargetDevCfg;

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_device_configure(pState->pBus,
                                                                &pState->sDeviceID,
                                                                &sTargetDevCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI device.\n");
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    aps_z8_cov_aipcfg(pState, pConfig, &pState->sDeviceCfgs.sAIPCfg);
    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                                       &pState->sDeviceCfgs.sAIPCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI device.\n");
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

#if AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE
    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AUTO_POWER_CONFIG,
                                                       &g_sLpCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI automatic power.\n");
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
#endif

    if (aps_z8_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//   Reads the info of the external psram and returns the value.
//
//*****************************************************************************
static uint32_t
aps_z8_info(am_device_xspi_periph_handle_t *pPsram, am_devices_xspi_info_t *pStateInfo)
{
    aps_z8_state_t *pState = (aps_z8_state_t *)pPsram->pData;

    pStateInfo->ui8VendorId  = pState->sRegisters.MR1_b.VID;
    pStateInfo->ui8DeviceId  = pState->sRegisters.MR2_b.GENERATION;
    pStateInfo->ui32BaseAddr = pState->sDeviceCfgs.sAIPCfg.ui32AIPBaseAddr;
    pStateInfo->sCE          = pState->sDeviceID;

    pStateInfo->ui32DeviceSizeKb = aps_z8_device_size_get(pState) / 1024u;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

#if !AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE
//*****************************************************************************
//
// Enter half sleep
//
// Send a command to Enter Half Sleep Mode. Will need to Be in OCTAL mode
// to access the register per the device driver.
//
//*****************************************************************************
static uint32_t
aps_z8_enter_halfsleep(am_device_xspi_periph_handle_t *pPsram)
{
    aps_z8_state_t          *pState       = (aps_z8_state_t *)pPsram->pData;
    uint16_t                ui16PIOBuffer = 0x00F0;
    am_hal_xspimc_dev_cfg_t sTempCfg      = pState->sDeviceCfgs.sDevCfg;
    am_hal_xspimc_aip_cfg_t sTempAipCfg   = pState->sDeviceCfgs.sAIPCfg;

    //
    // Reconfigure XSPI to OCTAL
    //
    sTempCfg.eIOMode  = AM_HAL_XSPIMC_IO_OCTAL;
    sTempCfg.ui32Freq = XSPI_BASE_FREQUENCY;
    //
    // maintain 24MHz clock so that when exiting halfsleep, the dummy command CE low pulse is fixed.
    //

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_device_configure(pState->pBus,
                                                                &pState->sDeviceID,
                                                                &sTempCfg))
    {
        am_util_debug_printf("Error - Failed to configure XSPI device.\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sTempAipCfg.bAIPEnable = false;
    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                                       &sTempAipCfg))
    {
        am_util_debug_printf("Error - Failed to disable XSPI AIP.\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    //
    // Send command to Enter half sleep
    //
    if (AM_HAL_STATUS_SUCCESS != am_device_command_write(pState,
                                                         AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE_REGISTER,
                                                         AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                                         6,
                                                         (uint8_t *)&ui16PIOBuffer,
                                                         2))
    {
        am_util_debug_printf("Failed to write PSRAM MR6 register!\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_periph_delay(pPsram, APSZ8_tHS_MIN_US);

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
// This function resets the device to bring it out of halfsleep
//
//*****************************************************************************
static uint32_t
aps_z8_exit_halfsleep(am_device_xspi_periph_handle_t *pPsram)
{
    aps_z8_state_t *pState       = (aps_z8_state_t *)pPsram->pData;
    uint16_t       ui16PIOBuffer = 0;

    //
    // Send dummy command to pull CE for tXPHS
    //
    if (AM_HAL_STATUS_SUCCESS != am_device_command_write(pState, 0x0000,
                                                         AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN,
                                                         0, (uint8_t *)&ui16PIOBuffer, 2))
    {
        am_util_debug_printf("Error - Failed to send dummy command.\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    //
    // Delay after setting pin high to allow for device to accept command
    //  and go into half sleep mode
    //
    am_hal_periph_delay(pPsram, APSZ8_tXHS_MIN_US);

    //
    // Reconfigure XSPI
    //
    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_device_configure(pState->pBus,
                                                                &pState->sDeviceID,
                                                                &pState->sDeviceCfgs.sDevCfg))
    {
        am_util_debug_printf("Error - Failed to restore XSPI.\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                                       &pState->sDeviceCfgs.sAIPCfg))
    {
        am_util_debug_printf("Error - Failed to restore XSPI AIP.\n");
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
aps_z8_power(am_device_xspi_periph_handle_t *pPsram,
             am_devices_xspi_power_e ePowerMode,
             const am_devices_xspi_psram_aps_z8_power_cfg_t *pPowerCfg)
{
    aps_z8_state_t *pState = (aps_z8_state_t *)pPsram->pData;

    if (aps_z8_device_acquire(pPsram))
    {
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    switch(ePowerMode)
    {
        case AM_DEVICES_XSPI_POWER_ON:
            if (am_hal_xspimc_power_control(pState->pBus, &pState->sDeviceID, AM_HAL_XSPIMC_POWER_REQUEST_ON))
            {
                aps_z8_device_error_ret(pPsram);
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
            if (aps_z8_exit_halfsleep(pPsram))
            {
                aps_z8_device_error_ret(pPsram);
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
            break;

        case AM_DEVICES_XSPI_POWER_OFF:
            if (aps_z8_enter_halfsleep(pPsram))
            {
                aps_z8_device_error_ret(pPsram);
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
            if (am_hal_xspimc_power_control(pState->pBus, &pState->sDeviceID, AM_HAL_XSPIMC_POWER_REQUEST_OFF))
            {
                aps_z8_device_error_ret(pPsram);
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
            break;
        case AM_DEVICES_XSPI_POWER_AUTO:
            //am_hal_xspi configure auto (pState, pPowerCfg)
            aps_z8_device_error_ret(pPsram);
            return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (aps_z8_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}
#endif //!AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE

static uint32_t
aps_z8_mem_map_config(am_device_xspi_periph_handle_t *pPsram,
                      const am_devices_xspi_memmap_cfg_t *pCfg)
{
    if (!pCfg)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    aps_z8_state_t *pState = (aps_z8_state_t *)pPsram->pData;

    if (aps_z8_device_acquire(pPsram))
    {
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_xspimc_aip_cfg_t *pAipCfg = &pState->sDeviceCfgs.sAIPCfg;


    if (pCfg->bEnable)
    {
        pAipCfg->ui32AIPBaseAddr         = pCfg->ui32BaseAddr;
        pAipCfg->ui32MappedDevBaseOffset = pCfg->ui32MappedDevBaseOffset;
        pAipCfg->ui32AIPSize             = aps_z8_aip_size_clamp(pState,
                                                                 pCfg->ui32Size);
        pAipCfg->bPrefetch               = pCfg->bPrefetch;
    }
    pAipCfg->ui8WindowNum = pCfg->ui8WindowNum;
    pAipCfg->bAIPEnable   = pCfg->bEnable;

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                                       pAipCfg))
    {
        aps_z8_device_error_ret(pPsram);
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
            aps_z8_device_error_ret(pPsram);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }*/
    }

    if (aps_z8_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Initialize the xspi_psram driver.
//
//*****************************************************************************
uint32_t
am_devices_xspi_psram_aps_z8_init(am_device_xspi_periph_handle_t *pPsram)
{
    aps_z8_state_t *pState;
    uint32_t       ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    if (pPsram->pData == NULL ||
        pPsram->uintSize < sizeof(aps_z8_state_t))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_hal_periph_preinit(pPsram);
    if ( ui32Status )
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    // other initializations

    //
    // Global resource init
    //
    pState = (aps_z8_state_t *)pPsram->pData;
    memset(pState, 0, sizeof(aps_z8_state_t));

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

    aps_z8_default_set(pState);

    pPsram->state.bInitialized = true;
    pPsram->state.ui8InitRet   = ui32Status;

    //
    // Return the status.
    //
    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
// DeInitialize the xspi_psram driver.
//
//*****************************************************************************
uint32_t
am_devices_xspi_psram_aps_z8_deinit(am_device_xspi_periph_handle_t *pPsram)
{
    uint32_t ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    if (aps_z8_device_acquire(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    aps_z8_state_t *pState = (aps_z8_state_t *)pPsram->pData;

    ui32Status = am_hal_xspimc_power_control(pState->pBus,
                                             &pState->sDeviceID,
                                             AM_HAL_XSPIMC_POWER_REQUEST_OFF);
    if (ui32Status)
    {
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (aps_z8_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_hal_periph_postdeinit(pPsram);
    if ( ui32Status )
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pPsram->state.bInitialized = false;
    pPsram->state.ui8InitRet   = ui32Status;

    //
    // Return the status.
    //
    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_psram_aps_z8_timing_apply(am_device_xspi_periph_handle_t *pPsram,
                                          const am_devices_xspi_timing_t *pCfg)
{
    if (!pCfg)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    aps_z8_state_t *pState = (aps_z8_state_t *)pPsram->pData;

    if (aps_z8_device_acquire(pPsram))
    {
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_TIMING_CONFIG,
                                                       pCfg))
    {
        aps_z8_device_error_ret(pPsram);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState->sDeviceCfgs.sTimingCfg = *pCfg;

    if (aps_z8_device_release(pPsram))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

const am_device_xspi_func_t apiAPSZ8 =
{
    .device_init           = am_devices_xspi_psram_aps_z8_init,
    .device_deinit         = am_devices_xspi_psram_aps_z8_deinit,
    .device_config         = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                           const am_devices_xspi_config_t *,
                                           ...)) aps_z8_config,
    .device_get_info       = aps_z8_info,
    .device_write          = NULL,
    .device_read           = NULL,
    .device_erase          = NULL,
    .device_set_xfer_mode  = NULL,
#if AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE
    .device_power_ctrl     = NULL,
#else
    .device_power_ctrl     = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                          am_devices_xspi_power_e,
                                          ...)) aps_z8_power,
#endif
    .device_mem_map_config = aps_z8_mem_map_config,
    //TODO: add timing scan function when available
    //.device_timing_scan  = am_devices_xspi_timing_scan,
    .device_timing_apply   = am_devices_xspi_psram_aps_z8_timing_apply,
};

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
