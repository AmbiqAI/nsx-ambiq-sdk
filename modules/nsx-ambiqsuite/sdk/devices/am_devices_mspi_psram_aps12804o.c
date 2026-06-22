//*****************************************************************************
//
//! @file am_devices_mspi_psram_aps12804o.c
//!
//! @brief Multi-bit SPI PSRAM driver for the APS12804O device.
//!
//! @addtogroup devices_mspi_psram_aps12804o APS12804O MSPI PSRAM Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a hardware abstraction layer
//!          for the APS12804O Multi-bit SPI PSRAM device. It enables high-speed
//!          DDR read/write operations, XIP functionality, and power management for
//!          embedded applications requiring external volatile memory. The driver
//!          supports efficient data access, timing optimization, scrambling,
//!          and system integration for optimal PSRAM performance.
//!
//! @section devices_mspi_psram_aps12804o_features Key Features
//!
//! 1. @b High-speed @b DDR @b Access: Quad SPI DDR mode for maximum throughput.
//! 2. @b XIP @b Mode: Execute code directly from PSRAM memory space.
//! 3. @b DMA @b Support: Efficient non-blocking data transfer operations.
//! 4. @b Scrambling @b Support: Hardware-based data scrambling for security.
//! 5. @b Power @b Management: Half-sleep mode for reduced power consumption.
//! 6. @b Timing @b Calibration: Automatic timing optimization and validation.
//! 7. @b High-priority @b Operations: Support for priority-based read/write access.
//!
//! @section devices_mspi_psram_aps12804o_functionality Functionality
//!
//! - Initialize and configure APS12804O device with DDR timing
//! - Perform blocking and non-blocking read/write operations
//! - Handle advanced read/write operations with DMA support
//! - Manage XIP mode configuration and control
//! - Control data scrambling for enhanced security
//! - Implement power management with half-sleep modes
//! - Calibrate and optimize timing parameters
//! - Support high-priority memory access operations
//!
//! @section devices_mspi_psram_aps12804o_usage Usage
//!
//! 1. Initialize device with am_devices_mspi_psram_aps12804o_ddr_init()
//! 2. Perform timing calibration with am_devices_mspi_psram_aps12804o_ddr_init_timing_check()
//! 3. Read/write data using blocking APIs (ddr_read/ddr_write) or non-blocking APIs
//! 4. Enable XIP mode with am_devices_mspi_psram_aps12804o_ddr_enable_xip()
//! 5. Configure scrambling if needed with enable/disable scrambling functions
//! 6. Use power management features like half-sleep mode for energy efficiency
//! 7. Deinitialize with am_devices_mspi_psram_aps12804o_ddr_deinit() when done
//!
//! @section devices_mspi_psram_aps12804o_configuration Configuration
//!
//! - Set up MSPI interface with DDR timing parameters
//! - Configure DMA for high-performance data transfers
//! - Enable DDR mode with optimized timing settings
//! - Configure scrambling parameters for secure operations
//! - Set up power management modes and wake-up sequences
//! - Optimize timing calibration for different operating conditions
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
#include "am_mcu_apollo.h"
#include "am_devices_mspi_psram_aps12804o.h"
#include "am_bsp.h"
#include "am_util.h"

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************
// #### INTERNAL BEGIN ####
#if defined (APOLLO5_FPGA)
#define USE_NON_DQS_MODE
#else
// #### INTERNAL END ####
// #define USE_NON_DQS_MODE
// #### INTERNAL BEGIN ####
#endif //defined (APOLLO5_FPGA)
// #### INTERNAL END ####

#define MSPI_BASE_FREQUENCY AM_HAL_MSPI_CLK_48MHZ

#define APS12804O_tHS_MIN_US        155 // with margin
#define APS12804O_tXHS_MIN_US       155 // with margin
#define APS12804O_tPU_MIN_US        155 // with margin

#define AM_DEVICES_MSPI_PSRAM_TIMEOUT             1000000
#if defined(AM_PART_APOLLO5_API)
#define PSRAM_TIMING_SCAN_MIN_ACCEPTANCE_LENGTH   (3)     // there should be at least
                                                          // this amount of consecutive
                                                          // passing settings to be accepted.
#elif defined(AM_PART_APOLLO4_API)
#define PSRAM_TIMING_SCAN_MIN_ACCEPTANCE_LENGTH   (8)     // there should be at least
                                                          // this amount of consecutive
                                                          // passing settings to be accepted.
#endif

am_hal_mspi_xip_config_t gAPS128DDRXipConfig[] =
{
  {
    .ui32APBaseAddr       = MSPI0_APERTURE_START_ADDR,
    .eAPMode              = AM_HAL_MSPI_AP_READ_WRITE,
    .eAPSize              = AM_HAL_MSPI_AP_SIZE16M,
    .scramblingStartAddr  = 0,
    .scramblingEndAddr    = 0,
  },
  {
    .ui32APBaseAddr       = MSPI1_APERTURE_START_ADDR,
    .eAPMode              = AM_HAL_MSPI_AP_READ_WRITE,
    .eAPSize              = AM_HAL_MSPI_AP_SIZE16M,
    .scramblingStartAddr  = 0,
    .scramblingEndAddr    = 0,
  },
  {
    .ui32APBaseAddr       = MSPI2_APERTURE_START_ADDR,
    .eAPMode              = AM_HAL_MSPI_AP_READ_WRITE,
    .eAPSize              = AM_HAL_MSPI_AP_SIZE16M,
    .scramblingStartAddr  = 0,
    .scramblingEndAddr    = 0,
  },
#if AM_REG_MSPI_NUM_MODULES == 4
  {
    .ui32APBaseAddr       = MSPI3_APERTURE_START_ADDR,
    .eAPMode              = AM_HAL_MSPI_AP_READ_WRITE,
    .eAPSize              = AM_HAL_MSPI_AP_SIZE16M,
    .scramblingStartAddr  = 0,
    .scramblingEndAddr    = 0,
  }
#endif
};

am_hal_mspi_dqs_t gAPS128DDRDqsCfg[] =
{
  {
#ifdef USE_NON_DQS_MODE
    .bDQSEnable             = 0,
#else
    .bDQSEnable             = 1,
#endif
    .bDQSSyncNeg            = 0,
    .bEnableFineDelay       = 0,
    .ui8TxDQSDelay          = 0,
    .ui8RxDQSDelay          = 16,
    .ui8RxDQSDelayNeg       = 0,
    .bRxDQSDelayNegEN       = 0,
    .ui8RxDQSDelayHi        = 0,
    .ui8RxDQSDelayNegHi     = 0,
    .bRxDQSDelayHiEN        = 0,
  },
  {
#ifdef USE_NON_DQS_MODE
    .bDQSEnable             = 0,
#else
    .bDQSEnable             = 1,
#endif
    .bDQSSyncNeg            = 0,
    .bEnableFineDelay       = 0,
    .ui8TxDQSDelay          = 0,
    .ui8RxDQSDelay          = 16,
    .ui8RxDQSDelayNeg       = 0,
    .bRxDQSDelayNegEN       = 0,
    .ui8RxDQSDelayHi        = 0,
    .ui8RxDQSDelayNegHi     = 0,
    .bRxDQSDelayHiEN        = 0,
  },
  {
#ifdef USE_NON_DQS_MODE
    .bDQSEnable             = 0,
#else
    .bDQSEnable             = 1,
#endif
    .bDQSSyncNeg            = 0,
    .bEnableFineDelay       = 0,
#if defined(AM_PART_APOLLO4P)
    .ui8TxDQSDelay          = 12,
#else
    .ui8TxDQSDelay          = 0,
#endif
    .ui8RxDQSDelay          = 16,
    .ui8RxDQSDelayNeg       = 0,
    .bRxDQSDelayNegEN       = 0,
    .ui8RxDQSDelayHi        = 0,
    .ui8RxDQSDelayNegHi     = 0,
    .bRxDQSDelayHiEN        = 0,
  },
#if defined(AM_PART_APOLLO5_API)
  {
#ifdef USE_NON_DQS_MODE
    .bDQSEnable             = 0,
#else
    .bDQSEnable             = 1,
#endif
    .bDQSSyncNeg            = 0,
    .bEnableFineDelay       = 0,
    .ui8TxDQSDelay          = 0,
    .ui8RxDQSDelay          = 16,
    .ui8RxDQSDelayNeg       = 0,
    .bRxDQSDelayNegEN       = 0,
    .ui8RxDQSDelayHi        = 0,
    .ui8RxDQSDelayNegHi     = 0,
    .bRxDQSDelayHiEN        = 0,
  }
#endif
};

am_hal_mspi_xip_misc_t gAPS128XipMiscCfg[] =
{
  {
    .ui32CEBreak        = 10,
    .bXIPBoundary       = true,
    .bXIPOdd            = true,
    .bAppndOdd          = false,
    .bBEOn              = false,
    .eBEPolarity        = AM_HAL_MSPI_BE_LOW_ENABLE,
  },
  {
    .ui32CEBreak        = 10,
    .bXIPBoundary       = true,
    .bXIPOdd            = true,
    .bAppndOdd          = false,
    .bBEOn              = false,
    .eBEPolarity        = AM_HAL_MSPI_BE_LOW_ENABLE,
  },
  {
    .ui32CEBreak        = 10,
    .bXIPBoundary       = true,
    .bXIPOdd            = true,
    .bAppndOdd          = false,
    .bBEOn              = false,
    .eBEPolarity        = AM_HAL_MSPI_BE_LOW_ENABLE,
  },
#if defined(AM_PART_APOLLO5_API)
  {
    .ui32CEBreak        = 10,
    .bXIPBoundary       = true,
    .bXIPOdd            = true,
    .bAppndOdd          = false,
    .bBEOn              = false,
    .eBEPolarity        = AM_HAL_MSPI_BE_LOW_ENABLE,
  }
#endif
};

am_hal_mspi_config_t gAPS128DDRMspiCfg[] =
{
  {
    .ui32TCBSize          = 0,
    .pTCB                 = NULL,
    .bClkonD4             = 0
  },
  {
    .ui32TCBSize          = 0,
    .pTCB                 = NULL,
    .bClkonD4             = 0
  },
  {
    .ui32TCBSize          = 0,
    .pTCB                 = NULL,
    .bClkonD4             = 0
  },
#if defined(AM_PART_APOLLO5_API)
  {
    .ui32TCBSize          = 0,
    .pTCB                 = NULL,
    .bClkonD4             = 0
  },
#endif
};

am_devices_mspi_psram_ddr_timing_config_t gAPS128TimingCfg[] =
{
  {
    {
// #### INTERNAL BEGIN ####
#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO330P_510L)
      .bTxDataDelay = false,
#endif
// #### INTERNAL END ####
      .bTxNeg = 1,
      .bRxNeg = 0,
      .bRxCap = 0,
      .ui8TxDQSDelay = 0,
      .ui8RxDQSDelay = 0,
      .ui8Turnaround = 6,
    },
    .bValid = false,
  },
  {
    {
// #### INTERNAL BEGIN ####
#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO330P_510L)
      .bTxDataDelay = false,
#endif
// #### INTERNAL END ####
      .bTxNeg = 1,
      .bRxNeg = 0,
      .bRxCap = 0,
      .ui8TxDQSDelay = 0,
      .ui8RxDQSDelay = 0,
      .ui8Turnaround = 6,
    },
    .bValid = false,
  },
  {
    {
// #### INTERNAL BEGIN ####
#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO330P_510L)
      .bTxDataDelay = false,
#endif
// #### INTERNAL END ####
      .bTxNeg = 1,
      .bRxNeg = 0,
      .bRxCap = 0,
      .ui8TxDQSDelay = 0,
      .ui8RxDQSDelay = 0,
      .ui8Turnaround = 6,
    },
    .bValid = false,
  },
#if defined(AM_PART_APOLLO5_API)
  {
    {
// #### INTERNAL BEGIN ####
#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO330P_510L)
      .bTxDataDelay = false,
#endif
// #### INTERNAL END ####
      .bTxNeg = 1,
      .bRxNeg = 0,
      .bRxCap = 0,
      .ui8TxDQSDelay = 0,
      .ui8RxDQSDelay = 0,
      .ui8Turnaround = 6,
    },
    .bValid = false,
  },
#endif
};

am_hal_mspi_rxcfg_t gAPS128MspiRxCfg =
{
    .ui8DQSturn         = 2,
    .bRxHI              = 0,
    .bTaForth           = 1,
    .bHyperIO           = 0,
    .ui8RxSmp           = 1,
    .bRBX               = 0,
    .bWBX               = 0,
    .bSCLKRxHalt        = 0,
    .bRxCapEXT          = 0,
    .ui8Sfturn          = 0,
};

am_hal_mspi_dev_config_t  APS128DDRQuadMSPIConfig =
{
  .eAddrCfg             = AM_HAL_MSPI_ADDR_3_BYTE,
  .eInstrCfg            = AM_HAL_MSPI_INSTR_2_BYTE,
  .ui16ReadInstr        = AM_DEVICES_MSPI_PSRAM_DDR_READ,
  .ui16WriteInstr       = AM_DEVICES_MSPI_PSRAM_DDR_WRITE,
  .eDeviceConfig        = AM_HAL_MSPI_FLASH_QUAD_DDR_CE0,
  .eSpiMode             = AM_HAL_MSPI_SPI_MODE_0,
  .bSendAddr            = true,
  .bSendInstr           = true,
  .bTurnaround          = true,
  .eClockFreq           = AM_HAL_MSPI_CLK_96MHZ,
#ifdef USE_NON_DQS_MODE
  .ui8TurnAround        = 9,
  .ui8WriteLatency      = 4,
#else
  .ui8TurnAround        = 5,
  .ui8WriteLatency      = 4,
#endif
  .bEnWriteLatency      = true,
  .bEmulateDDR          = true,
#if defined(AM_PART_APOLLO5_API)
  .eCeLatency           = AM_HAL_MSPI_CE_LATENCY_NORMAL,
#endif
// #### INTERNAL BEGIN ####
#if defined(APOLLO4_FPGA) || defined(APOLLO5_FPGA)
  .ui16DMATimeLimit     = 40,
#else
// #### INTERNAL END ####
  .ui16DMATimeLimit     = 40,
// #### INTERNAL BEGIN ####
#endif
// #### INTERNAL END ####
  .eDMABoundary         = AM_HAL_MSPI_BOUNDARY_BREAK1K,
};

typedef struct
{
    uint32_t                                ui32Module;     //MSPI instance number
    void                                    *pMspiHandle;   //MSPI instance handle
    am_hal_mspi_dev_config_t                stSetting;      //MSPI instance setting
    bool                                    bOccupied;      //Is device occupied
    am_devices_mspi_psram_aps12804o_regs_t  sRegisters;     //Device registers

    am_hal_mspi_config_t                        *pMspiCfg;      //pointer to global storage
    am_hal_mspi_xip_config_t                    *pXipCfg;       //pointer to global storage
    am_hal_mspi_xip_misc_t                      *pXipMiscCfg;   //pointer to global storage
    am_hal_mspi_dqs_t                           *pDqsCfg;       //pointer to global storage
    am_hal_mspi_rxcfg_t                         *pRxCfg;        //pointer to global storage
    am_devices_mspi_psram_ddr_timing_config_t   *pTimingCfg;    //pointer to global storage
} am_devices_mspi_psram_t;

am_devices_mspi_psram_t gAPS128DDRPsram[AM_DEVICES_MSPI_PSRAM_MAX_DEVICE_NUM];

static uint32_t
am_devices_psram_aps12804o_default_regs_set(am_devices_mspi_psram_t *pPsram)
{
    memset(&pPsram->sRegisters, 0, sizeof(pPsram->sRegisters));

    pPsram->sRegisters.MR0_b.DS = AM_DEVICES_MSPI_PSRAM_APS_E8_DRIVE_STRENGTH_50;
    pPsram->sRegisters.MR0_b.LC = AM_DEVICES_MSPI_PSRAM_APS_E8_WLC_4;
    pPsram->sRegisters.MR0_b.BL = 0x3;
    pPsram->sRegisters.MR0_b.DM = 0;

    return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

static inline uint8_t am_devices_mspi_psram_aps_e8_get_rlc(am_devices_mspi_psram_aps_e8_rlc_e eRLC, uint8_t *pRLC)
{
    switch(eRLC)
    {
      case AM_DEVICES_MSPI_PSRAM_APS_E8_RLC_3:
          *pRLC = 3;
          break;
      case AM_DEVICES_MSPI_PSRAM_APS_E8_RLC_4:
          *pRLC = 4;
          break;
      case AM_DEVICES_MSPI_PSRAM_APS_E8_RLC_5:
          *pRLC = 5;
          break;
      case AM_DEVICES_MSPI_PSRAM_APS_E8_RLC_6:
          *pRLC = 6;
          break;
      default:
          return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
    return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

static void pfnMSPI_APMPSRAM_DDR_Callback(void *pCallbackCtxt, uint32_t status)
{
#if defined(AM_PART_APOLLO5_API)
    //
    // Flush and invalidate whole cache
    // Recommend user to manage cache coherency based on application usage
    //
    am_hal_cachectrl_dcache_invalidate(NULL, true);
#endif
    //
    // Set the DMA complete flag.
    //
    *(volatile uint32_t *)pCallbackCtxt = status;
}

//*****************************************************************************
//
//! @brief Generic Command Write function.
//!
//! @param pMspiHandle
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
am_device_command_write(void *pMspiHandle,
                        uint16_t ui16Instr,
                        bool bSendAddr,
                        uint32_t ui32Addr,
                        uint32_t *pData,
                        uint32_t ui32NumBytes)
{
  am_hal_mspi_pio_transfer_t  Transaction;
  //
  // Create the individual write transaction.
  //
  Transaction.ui32NumBytes            = ui32NumBytes;
  Transaction.bScrambling             = false;
  Transaction.eDirection              = AM_HAL_MSPI_TX;
  Transaction.bSendAddr               = bSendAddr;
  Transaction.ui32DeviceAddr          = ui32Addr;
  Transaction.bSendInstr              = true;
  Transaction.ui16DeviceInstr         = ui16Instr;
  Transaction.bTurnaround             = false;
  Transaction.bDCX                    = false;
  Transaction.bEnWRLatency            = false;
// #### INTERNAL BEGIN ####
// FALCSW-426 7/29/22 Deprecate MSPI CONT bit. (See also A3DS-25.)
// #### INTERNAL END ####
  Transaction.bContinue               = false;  // MSPI CONT is deprecated for Apollo4
  Transaction.pui32Buffer             = pData;
  //
  // Execute the transction over MSPI.
  //
  return am_hal_mspi_blocking_transfer(pMspiHandle,
                                       &Transaction,
                                       AM_DEVICES_MSPI_PSRAM_TIMEOUT);
}

//*****************************************************************************
//
//! @brief Generic Command Read function.
//!
//! @param pMspiHandle
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
am_device_command_read(void *pMspiHandle,
                       uint16_t ui16Instr,
                       bool bSendAddr,
                       uint32_t ui32Addr,
                       uint32_t *pData,
                       uint32_t ui32NumBytes)
{
  am_hal_mspi_pio_transfer_t  Transaction;
  //
  // Create the individual write transaction.
  //
  Transaction.ui32NumBytes            = ui32NumBytes;
  Transaction.bScrambling             = false;
  Transaction.eDirection              = AM_HAL_MSPI_RX;
  Transaction.bSendAddr               = bSendAddr;
  Transaction.ui32DeviceAddr          = ui32Addr;
  Transaction.bSendInstr              = true;
  Transaction.ui16DeviceInstr         = ui16Instr;
  Transaction.bTurnaround             = true;
  Transaction.bDCX                    = false;
  Transaction.bEnWRLatency            = true;
// #### INTERNAL BEGIN ####
// FALCSW-426 7/29/22 Deprecate MSPI CONT bit. (See also A3DS-25.)
// #### INTERNAL END ####
  Transaction.bContinue               = false;  // MSPI CONT is deprecated for Apollo4
  Transaction.pui32Buffer             = pData;
  //
  // Execute the transction over MSPI.
  //
  return am_hal_mspi_blocking_transfer(pMspiHandle,
                                       &Transaction,
                                       AM_DEVICES_MSPI_PSRAM_TIMEOUT);
}

//*****************************************************************************
//
//! @brief Reset the external psram
//!
//! @param pPsram
//!
//! @return
//
//*****************************************************************************
static uint32_t
am_devices_mspi_psram_aps12804o_reset(am_devices_mspi_psram_t *pPsram)
{

  if (AM_HAL_STATUS_SUCCESS != am_device_command_write(pPsram->pMspiHandle, AM_DEVICES_MSPI_PSRAM_DDR_RSTEN, false, 0, NULL, 0))
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  if (AM_HAL_STATUS_SUCCESS != am_device_command_write(pPsram->pMspiHandle, AM_DEVICES_MSPI_PSRAM_DDR_RST, false, 0, NULL, 0))
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  am_util_delay_us(APS12804O_tPU_MIN_US);

  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

static uint32_t
am_devices_mspi_peripheral_init(uint32_t ui32Module,
                                am_devices_mspi_psram_config_t *pDevCfg,
                                am_devices_mspi_psram_t *pPsram)
{
  uint32_t    ui32Status;

  if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_initialize(ui32Module, &pPsram->pMspiHandle))
  {
      am_util_debug_printf("Error - Failed to initialize MSPI.\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_power_control(pPsram->pMspiHandle, AM_HAL_SYSCTRL_WAKE, false))
  {
      am_util_debug_printf("Error - Failed to power on MSPI.\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  pPsram->pMspiCfg->ui32TCBSize = pDevCfg->ui32NBTxnBufLength;
  pPsram->pMspiCfg->pTCB = pDevCfg->pNBTxnBuf;
  if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_configure(pPsram->pMspiHandle, pPsram->pMspiCfg))
  {
      am_util_debug_printf("Error - Failed to configure MSPI device.\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  pPsram->stSetting = APS128DDRQuadMSPIConfig;

  switch (pDevCfg->eDeviceConfig)
  {
      case AM_HAL_MSPI_FLASH_QUAD_DDR_CE0:
      case AM_HAL_MSPI_FLASH_QUAD_DDR_CE1:
        pPsram->stSetting.eDeviceConfig = pDevCfg->eDeviceConfig;
        break;
      default:
          return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  pPsram->stSetting.eClockFreq = MSPI_BASE_FREQUENCY;

  if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS !=
      am_devices_mspi_psram_aps_e8_get_rlc((am_devices_mspi_psram_aps_e8_rlc_e)pPsram->sRegisters.MR0_b.LC,
                                            &pPsram->stSetting.ui8TurnAround) )
  {
      am_util_debug_printf("Invalid Read Latency Code!\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  pPsram->stSetting.ui8WriteLatency = 2 * pPsram->sRegisters.MR0_b.LC;

  if (!pPsram->pDqsCfg->bDQSEnable)
  {
    pPsram->stSetting.ui8TurnAround = pPsram->sRegisters.MR0_b.LC * 2 + 1;
  }
  else
  {
    pPsram->stSetting.ui8TurnAround = pPsram->sRegisters.MR0_b.LC + 1;
  }

  if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_device_configure(pPsram->pMspiHandle, &pPsram->stSetting))
  {
      am_util_debug_printf("Error - Failed to configure MSPI device.\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  pPsram->pXipCfg->scramblingStartAddr = pDevCfg->ui32ScramblingStartAddr;
  pPsram->pXipCfg->scramblingEndAddr = pDevCfg->ui32ScramblingEndAddr;
  ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_XIP_CONFIG, pPsram->pXipCfg);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_XIP_MISC_CONFIG, pPsram->pXipMiscCfg);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_DQS, pPsram->pDqsCfg);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_RXCFG, pPsram->pRxCfg);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  //
  // Enable DDR emulation in MSPI
  //
  ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_DDR_EN, NULL);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_enable(pPsram->pMspiHandle))
  {
      am_util_debug_printf("Error - Failed to enable MSPI.\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  am_bsp_mspi_pins_enable(ui32Module, pPsram->stSetting.eDeviceConfig);
  am_util_delay_us(150);

  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

static uint32_t
am_devices_mspi_psram_aps12804o_device_init(am_devices_mspi_psram_t *pPsram, am_hal_mspi_clock_e eTargetFreq)
{
  uint32_t     ui32Status;
  uint32_t     ui32Rawdata;
  uint8_t      ui8RLCReg = 0;

  //
  // Send reset command to PSRAM
  //
  if (AM_HAL_STATUS_SUCCESS != am_devices_mspi_psram_aps12804o_reset(pPsram))
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  //
  // Read PSRAM Register MR0
  //
  am_util_debug_printf("Read PSRAM Register MR0\n");
  ui32Status = am_device_command_read(pPsram->pMspiHandle, AM_DEVICES_MSPI_PSRAM_DDR_READ_REGISTER, true, 0, &ui32Rawdata, 4);
  if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != ui32Status)
  {
      am_util_debug_printf("Failed to read PSRAM Register MR0!\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  else
  {
      pPsram->sRegisters.MR0 = (uint8_t)ui32Rawdata;
      am_util_debug_printf("PSRAM Register MR0  = 0x%02X\n", pPsram->sRegisters.MR0);
      am_util_debug_printf("PSRAM Read Latency  = %d\n", pPsram->sRegisters.MR0_b.LC + 1);
      am_util_debug_printf("PSRAM Write Latency = %d\n", pPsram->sRegisters.MR0_b.LC);
  }

  switch(eTargetFreq)
  {
#if defined (AM_PART_APOLLO5_API)
    case AM_HAL_MSPI_CLK_250MHZ:
      pPsram->sRegisters.MR0_b.LC = AM_DEVICES_MSPI_PSRAM_APS_E8_WLC_4;
      break;
    case AM_HAL_MSPI_CLK_192MHZ:
      pPsram->sRegisters.MR0_b.LC = AM_DEVICES_MSPI_PSRAM_APS_E8_WLC_3;
      break;
    case AM_HAL_MSPI_CLK_125MHZ:
      pPsram->sRegisters.MR0_b.LC = AM_DEVICES_MSPI_PSRAM_APS_E8_WLC_2;
      break;
#endif
    default:
      pPsram->sRegisters.MR0_b.LC = AM_DEVICES_MSPI_PSRAM_APS_E8_WLC_2;
      break;
  }
// #### INTERNAL BEGIN ####
#if defined(APOLLO5_FPGA) || defined(APOLLO4_FPGA)
  pPsram->sRegisters.MR0_b.LC = AM_DEVICES_MSPI_PSRAM_APS_E8_WLC_2;
#endif
// #### INTERNAL END ####

  ui32Rawdata = pPsram->sRegisters.MR0;
  ui32Status = am_device_command_write(pPsram->pMspiHandle, AM_DEVICES_MSPI_PSRAM_DDR_WRITE_REGISTER, true, 0, &ui32Rawdata, 1);
  if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != ui32Status)
  {
      am_util_debug_printf("\nFailed to write PSRAM Register MR0!\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  else
  {
      am_util_debug_printf("\nSet PSRAM Register MR0 into 0x%02X\n", pPsram->sRegisters.MR0);
  }

if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS !=
      am_devices_mspi_psram_aps_e8_get_rlc((am_devices_mspi_psram_aps_e8_rlc_e)pPsram->sRegisters.MR0_b.LC,
                                            &pPsram->stSetting.ui8TurnAround) )
  {
      am_util_debug_printf("Invalid Read Latency Code!\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  pPsram->stSetting.ui8WriteLatency = 2 * pPsram->sRegisters.MR0_b.LC;
  if (!pPsram->pDqsCfg->bDQSEnable)
  {
    pPsram->stSetting.ui8TurnAround = pPsram->sRegisters.MR0_b.LC * 2 + 1;
  }
  else
  {
    pPsram->stSetting.ui8TurnAround = pPsram->sRegisters.MR0_b.LC + 1;
  }

  if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_device_configure(pPsram->pMspiHandle, &pPsram->stSetting))
  {
      am_util_debug_printf("Error - Failed to configure MSPI device.\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  am_util_debug_printf("Read PSRAM MR0\n");
  ui32Status = am_device_command_read(pPsram->pMspiHandle, AM_DEVICES_MSPI_PSRAM_DDR_READ_REGISTER, true, 0, &ui32Rawdata, 1);
  if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != ui32Status)
  {
      am_util_debug_printf("Read PSRAM MR0!\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  else
  {
      ui8RLCReg = (uint8_t)ui32Rawdata;
      if ( ui8RLCReg == pPsram->sRegisters.MR0 )
      {
        am_util_debug_printf("PSRAM Register MR0  = 0x%02X\n", pPsram->sRegisters.MR0);
        am_util_debug_printf("PSRAM Read Latency  = %d\n", pPsram->sRegisters.MR0_b.LC + 1);
        am_util_debug_printf("PSRAM Write Latency = %d\n", pPsram->sRegisters.MR0_b.LC);
      }
      else
      {
        am_util_debug_printf("PSRAM Register MR0 = 0x%02X\nSet Fail\n\n", ui8RLCReg);
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
      }
  }

  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief This function takes care of splitting the transaction as needed, if the transaction crosses
//! PSRAM page boundary or because of tCEM restrictions, if hardware does not support it
//!
//! @param pPsram
//! @param bHiPrio
//! @param bWrite
//! @param pui8Buffer
//! @param ui32Address
//! @param ui32NumBytes
//! @param ui32PauseCondition
//! @param ui32StatusSetClr
//! @param pfnCallback
//! @param pCallbackCtxt
//!
//! @return
//
//*****************************************************************************
static uint32_t
psram_nonblocking_transfer(am_devices_mspi_psram_t *pPsram,
                           bool bHiPrio,
                           bool bWrite,
                           uint8_t *pui8Buffer,
                           uint32_t ui32Address,
                           uint32_t ui32NumBytes,
                           uint32_t ui32PauseCondition,
                           uint32_t ui32StatusSetClr,
                           am_hal_mspi_callback_t pfnCallback,
                           void *pCallbackCtxt)
{
  uint32_t ui32Status = AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
  am_hal_mspi_dma_transfer_t    Transaction;
  //
  // Set the DMA priority
  //
  Transaction.ui8Priority = 1;

  //
  // Set the transfer direction to RX (Read)
  //
  Transaction.eDirection = bWrite ? AM_HAL_MSPI_TX: AM_HAL_MSPI_RX;

  //
  // Initialize the CQ stimulus.
  //
  Transaction.ui32PauseCondition = ui32PauseCondition;
  //
  // Initialize the post-processing
  //
  Transaction.ui32StatusSetClr = 0;
  //
  // Need to be aware of page size
  //
  while (ui32NumBytes)
  {
    uint32_t size;
    if ((ui32Address & 0x3) &&
        ((AM_DEVICES_MSPI_PSRAM_PAGE_SIZE - (ui32Address & (AM_DEVICES_MSPI_PSRAM_PAGE_SIZE - 1))) < ui32NumBytes))
    {
      //
      // Hardware does not support Page splitting if address is not word aligned
      // Need to split the transaction
      //
      size = 4 - (ui32Address & 0x3);
    }
    else
    {
      size = ui32NumBytes;
    }

    bool bLast = (size == ui32NumBytes);
    //
    // Set the transfer count in bytes.
    //
    Transaction.ui32TransferCount = size;
    //
    // Set the address to read data from.
    //
    Transaction.ui32DeviceAddress = ui32Address;
    //
    // Set the target SRAM buffer address.
    //
    Transaction.ui32SRAMAddress = (uint32_t)pui8Buffer;

    if (bLast)
    {
      Transaction.ui32StatusSetClr = ui32StatusSetClr;
    }

    if (bHiPrio)
    {
      ui32Status = am_hal_mspi_highprio_transfer(pPsram->pMspiHandle, &Transaction, AM_HAL_MSPI_TRANS_DMA,
                                                 bLast ? pfnCallback : NULL,
                                                 bLast ? pCallbackCtxt : NULL);
    }
    else
    {
      ui32Status = am_hal_mspi_nonblocking_transfer(pPsram->pMspiHandle, &Transaction, AM_HAL_MSPI_TRANS_DMA,
                                                    bLast ? pfnCallback : NULL,
                                                    bLast ? pCallbackCtxt : NULL);
    }
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
      break;
    }
    ui32Address += size;
    ui32NumBytes -= size;
    pui8Buffer += size;

    Transaction.ui32PauseCondition = 0;
  }
  return ui32Status;
}

//*****************************************************************************
//
// Initialize the mspi_psram driver.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_init(uint32_t ui32Module,
                                         am_devices_mspi_psram_config_t *pDevCfg,
                                         void **ppHandle,
                                         void **ppMspiHandle)
{
    uint32_t                    ui32Status;
    am_hal_mspi_dev_config_t    *psConfig;
    am_devices_mspi_psram_t     *pPsram;

    if ((ui32Module > AM_REG_MSPI_NUM_MODULES) || (pDevCfg == NULL))
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

// #### INTERNAL BEGIN ####
    //
    // This driver is only for the Apollo5 famiy
    // There is no fault enable function for the apollo5a
    //
// #### INTERNAL END ####

    //
    // Allocate a vacant device handle
    //
    if ( gAPS128DDRPsram[ui32Module].bOccupied == false )
    {
        pPsram = &gAPS128DDRPsram[ui32Module];
        pPsram->pMspiCfg     = &gAPS128DDRMspiCfg[ui32Module];
        pPsram->pXipCfg      = &gAPS128DDRXipConfig[ui32Module];
        pPsram->pXipMiscCfg  = &gAPS128XipMiscCfg[ui32Module];
        pPsram->pDqsCfg      = &gAPS128DDRDqsCfg[ui32Module];
        pPsram->pRxCfg       = &gAPS128MspiRxCfg;
        pPsram->pTimingCfg   = &gAPS128TimingCfg[ui32Module];
        psConfig = &pPsram->stSetting;
        am_devices_psram_aps12804o_default_regs_set(pPsram);
    }
    else
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    am_util_debug_printf("\nStart PSRAM Initialization\n");

    //
    // Configure the MSPI into quad mode for PSRAM register access.
    //

    if (AM_HAL_STATUS_SUCCESS != am_devices_mspi_peripheral_init(ui32Module, pDevCfg, pPsram))
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != am_devices_mspi_psram_aps12804o_device_init(pPsram, pDevCfg->eClockFreq))
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    psConfig->eClockFreq = pDevCfg->eClockFreq;
#if defined(AM_PART_APOLLO330P_510L)
    psConfig->eCeLatency = AM_HAL_MSPI_CE_LATENCY_ADD1;
#endif

    switch (pDevCfg->eDeviceConfig)
    {
        case AM_HAL_MSPI_FLASH_QUAD_DDR_CE0:
        case AM_HAL_MSPI_FLASH_QUAD_DDR_CE1:
          psConfig->eDeviceConfig = pDevCfg->eDeviceConfig;
          break;

        default:
            return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Disable MSPI defore re-configuring it
    //
    ui32Status = am_hal_mspi_disable(pPsram->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

#if defined (AM_PART_APOLLO5_API)
    if (AM_HAL_MSPI_CLK_250MHZ == psConfig->eClockFreq
     || AM_HAL_MSPI_CLK_192MHZ == psConfig->eClockFreq
     || AM_HAL_MSPI_CLK_125MHZ == psConfig->eClockFreq
     || AM_HAL_MSPI_CLK_96MHZ == psConfig->eClockFreq)
    {
      pPsram->pRxCfg->ui8RxSmp = 2;
    }
#else
    if (AM_HAL_MSPI_CLK_96MHZ == psConfig->eClockFreq)
    {
      pPsram->pRxCfg->ui8RxSmp = 2;
    }
#endif
    ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_RXCFG, pPsram->pRxCfg);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Reconfig MSPI device settings
    //
    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_device_configure(pPsram->pMspiHandle, psConfig))
    {
        am_util_debug_printf("Error - Failed to reconfig MSPI device.\n");
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Re-Enable MSPI
    //
    ui32Status = am_hal_mspi_enable(pPsram->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Re-config the MSPI pins.
    //
    am_bsp_mspi_pins_enable(ui32Module, psConfig->eDeviceConfig);

    //
    // Enable MSPI interrupts.
    //
    ui32Status = am_hal_mspi_interrupt_clear(pPsram->pMspiHandle, AM_HAL_MSPI_INT_CQUPD | AM_HAL_MSPI_INT_ERR );
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    ui32Status = am_hal_mspi_interrupt_enable(pPsram->pMspiHandle, AM_HAL_MSPI_INT_CQUPD | AM_HAL_MSPI_INT_ERR );
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Return the handle.
    //
    pPsram->bOccupied = true;
    pPsram->ui32Module = ui32Module;
    *ppHandle = (void *)pPsram;
    *ppMspiHandle = pPsram->pMspiHandle;

    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
// DeInitialize the mspi_psram driver.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_deinit(void *pHandle)
{
    uint32_t    ui32Status;
    am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;

    if ( pHandle == NULL )
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    am_util_debug_printf("\nStart PSRAM Deinitialization\n");

    //
    // Disable and clear the interrupts to start with.
    //
    ui32Status = am_hal_mspi_interrupt_disable(pPsram->pMspiHandle, 0xFFFFFFFF);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
    ui32Status = am_hal_mspi_interrupt_clear(pPsram->pMspiHandle, 0xFFFFFFFF);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Disable MSPI instance.
    //
    ui32Status = am_hal_mspi_disable(pPsram->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
    //
    // Disable power to the MSPI instance.
    //
    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_power_control(pPsram->pMspiHandle, AM_HAL_SYSCTRL_DEEPSLEEP, false))
    {
        am_util_debug_printf("Error - Failed to power on MSPI.\n");
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
    //
    // Deinitialize the MPSI instance.
    //
    ui32Status = am_hal_mspi_deinitialize(pPsram->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
    //
    // Free this device handle
    //
    pPsram->bOccupied = false;

    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Reads the contents of the external PSRAM into a buffer.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_read(void *pHandle,
                                         uint8_t *pui8RxBuffer,
                                         uint32_t ui32ReadAddress,
                                         uint32_t ui32NumBytes,
                                         bool bWaitForCompletion)
{
  uint32_t                      ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  if (bWaitForCompletion)
  {
    //
    // Start the transaction.
    //
    volatile uint32_t ui32DMAStatus = 0xFFFFFFFF;
    ui32Status = psram_nonblocking_transfer(pPsram, false, false,
                                            pui8RxBuffer,
                                            ui32ReadAddress,
                                            ui32NumBytes,
                                            0,
                                            0,
                                            pfnMSPI_APMPSRAM_DDR_Callback,
                                            (void *)&ui32DMAStatus);
    //
    // Check the transaction status.
    //
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
    //
    // Wait for DMA Complete or Timeout
    //
    for (uint32_t i = 0; i < AM_DEVICES_MSPI_PSRAM_TIMEOUT; i++)
    {
      if (ui32DMAStatus != 0xFFFFFFFF)
      {
        break;
      }
      //
      // Call the BOOTROM cycle function to delay for about 1 microsecond.
      //
      am_hal_delay_us(1);
    }
    //
    // Check the status.
    //
    if (ui32DMAStatus != AM_HAL_STATUS_SUCCESS)
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
  }
  else
  {
    //
    // Check the transaction status.
    //
    ui32Status = psram_nonblocking_transfer(pPsram, false, false,
                                            pui8RxBuffer,
                                            ui32ReadAddress,
                                            ui32NumBytes,
                                            0,
                                            0,
                                            NULL,
                                            NULL);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
  }
  //
  // Return the status.
  //
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}
// #### INTERNAL BEGIN ####
//*****************************************************************************
//
//! @brief
//!
//! @param pHandle
//! @param pui8RxBuffer
//! @param ui32ReadAddress
//! @param ui32NumBytes
//!
//! @return
//
//*****************************************************************************
static uint32_t
mspi_aps12804o_ddr_dma_read(void *pHandle, uint8_t *pui8RxBuffer,
                            uint32_t ui32ReadAddress,
                            uint32_t ui32NumBytes)
{
  uint32_t                      ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;

    //
    // Start the transaction.
    //
    volatile uint32_t ui32DMAStatus = 0xFFFFFFFF;
    ui32Status = psram_nonblocking_transfer(pPsram, false, false,
                                            pui8RxBuffer,
                                            ui32ReadAddress,
                                            ui32NumBytes,
                                            0,
                                            0,
                                            pfnMSPI_APMPSRAM_DDR_Callback,
                                            (void *)&ui32DMAStatus);
    //
    // Check the transaction status.
    //
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
    //
    // Wait for DMA Complete or Timeout
    //
    for (uint32_t i = 0; i < AM_DEVICES_MSPI_PSRAM_TIMEOUT; i++)
    {
        //
        // check DMA status without using ISR
        //
        am_hal_mspi_interrupt_status_get(pPsram->pMspiHandle, &ui32Status, false);
        am_hal_mspi_interrupt_clear(pPsram->pMspiHandle, ui32Status);
        am_hal_mspi_interrupt_service(pPsram->pMspiHandle, ui32Status);

      if (ui32DMAStatus != 0xFFFFFFFF)
      {
        break;
      }
      //
      // Call the BOOTROM cycle function to delay for about 1 microsecond.
      //
      am_hal_delay_us(1);
    }
    //
    // Check the status.
    //
    if (ui32DMAStatus != AM_HAL_STATUS_SUCCESS)
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

  //
  // Return the status.
  //
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;

}
// #### INTERNAL END ####
//*****************************************************************************
//
// Reads the contents of the external PSRAM into a buffer.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_read_adv(void *pHandle,
                                             uint8_t *pui8RxBuffer,
                                             uint32_t ui32ReadAddress,
                                             uint32_t ui32NumBytes,
                                             uint32_t ui32PauseCondition,
                                             uint32_t ui32StatusSetClr,
                                             am_hal_mspi_callback_t pfnCallback,
                                             void *pCallbackCtxt)
{
  uint32_t                      ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  ui32Status = psram_nonblocking_transfer(pPsram, false, false,
                                          pui8RxBuffer,
                                          ui32ReadAddress,
                                          ui32NumBytes,
                                          ui32PauseCondition,
                                          ui32StatusSetClr,
                                          pfnCallback,
                                          pCallbackCtxt);
  //
  // Check the transaction status.
  //
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  //
  // Return the status.
  //
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Reads the contents of the external psram into a buffer
// with high priority
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_read_hiprio(void *pHandle,
                                                uint8_t *pui8RxBuffer,
                                                uint32_t ui32ReadAddress,
                                                uint32_t ui32NumBytes,
                                                am_hal_mspi_callback_t pfnCallback,
                                                void *pCallbackCtxt)
{
  uint32_t                      ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  ui32Status = psram_nonblocking_transfer(pPsram, true, false,
                                          pui8RxBuffer,
                                          ui32ReadAddress,
                                          ui32NumBytes,
                                          0,
                                          0,
                                          pfnCallback,
                                          pCallbackCtxt);
  //
  // Check the transaction status.
  //
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  //
  // Return the status.
  //
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

uint32_t
am_devices_mspi_psram_aps12804o_ddr_nonblocking_read(void *pHandle,
                                                     uint8_t *pui8RxBuffer,
                                                     uint32_t ui32ReadAddress,
                                                     uint32_t ui32NumBytes,
                                                     am_hal_mspi_callback_t pfnCallback,
                                                     void *pCallbackCtxt)
{
  uint32_t                      ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  //
  // Check the transaction status.
  //
  ui32Status = psram_nonblocking_transfer(pPsram, false, false,
                                          pui8RxBuffer,
                                          ui32ReadAddress,
                                          ui32NumBytes,
                                          0,
                                          0,
                                          pfnCallback,
                                          pCallbackCtxt);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  //
  // Return the status.
  //
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Programs the given range of psram addresses.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_write(void *pHandle,
                                          uint8_t *pui8TxBuffer,
                                          uint32_t ui32WriteAddress,
                                          uint32_t ui32NumBytes,
                                          bool bWaitForCompletion)
{
  uint32_t                      ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  if (bWaitForCompletion)
  {
    //
    // Start the transaction.
    //
    volatile uint32_t ui32DMAStatus = 0xFFFFFFFF;
    ui32Status = psram_nonblocking_transfer(pPsram, false, true,
                                            pui8TxBuffer,
                                            ui32WriteAddress,
                                            ui32NumBytes,
                                            0,
                                            0,
                                            pfnMSPI_APMPSRAM_DDR_Callback,
                                            (void *)&ui32DMAStatus);
    //
    // Check the transaction status.
    //
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
    //
    // Wait for DMA Complete or Timeout
    //
    for (uint32_t i = 0; i < AM_DEVICES_MSPI_PSRAM_TIMEOUT; i++)
    {
      if (ui32DMAStatus != 0xFFFFFFFF)
      {
        break;
      }
      //
      // Call the BOOTROM cycle function to delay for about 1 microsecond.
      //
      am_hal_delay_us(1);
    }
    //
    // Check the status.
    //
    if (ui32DMAStatus != AM_HAL_STATUS_SUCCESS)
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
  }
  else
  {
    //
    // Check the transaction status.
    //
    ui32Status = psram_nonblocking_transfer(pPsram, false, true,
                                            pui8TxBuffer,
                                            ui32WriteAddress,
                                            ui32NumBytes,
                                            0,
                                            0,
                                            NULL,
                                            NULL);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
  }

  //
  // Return the status.
  //
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}
// #### INTERNAL BEGIN ####
//*****************************************************************************
//
//! @brief
//!
//! @param pHandle
//! @param pui8TxBuffer
//! @param ui32WriteAddress
//! @param ui32NumBytes
//!
//! @return
//
//*****************************************************************************
static uint32_t
mspi_aps12804o_ddr_dma_write(void *pHandle, uint8_t *pui8TxBuffer,
                             uint32_t ui32WriteAddress,
                             uint32_t ui32NumBytes)
{
    uint32_t                      ui32Status;
    am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;

    {
        //
        // Start the transaction.
        //
        volatile uint32_t ui32DMAStatus = 0xFFFFFFFF;
        ui32Status = psram_nonblocking_transfer(pPsram, false, true,
                                    pui8TxBuffer,
                                    ui32WriteAddress,
                                    ui32NumBytes,
                                    0,
                                    0,
                                    pfnMSPI_APMPSRAM_DDR_Callback,
                                    (void *)&ui32DMAStatus);
        //
        // Check the transaction status.
        //
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
        }
        //
        // Wait for DMA Complete or Timeout
        //
        for (uint32_t i = 0; i < AM_DEVICES_MSPI_PSRAM_TIMEOUT; i++)
        {
            //
            // check DMA status without using ISR
            //
            am_hal_mspi_interrupt_status_get(pPsram->pMspiHandle, &ui32Status, false);
            am_hal_mspi_interrupt_clear(pPsram->pMspiHandle, ui32Status);
            am_hal_mspi_interrupt_service(pPsram->pMspiHandle, ui32Status);
            if (ui32DMAStatus != 0xFFFFFFFF)
            {
                break;
            }
            //
            // Call the BOOTROM cycle function to delay for about 1 microsecond.
            //
            am_hal_delay_us(1);
        }
        //
        // Check the status.
        //
        if (ui32DMAStatus != AM_HAL_STATUS_SUCCESS)
        {
            return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
        }
    }

    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;

}
// #### INTERNAL END ####
//*****************************************************************************
//
// Programs the given range of psram addresses.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_write_adv(void *pHandle,
                                              uint8_t *puiTxBuffer,
                                              uint32_t ui32WriteAddress,
                                              uint32_t ui32NumBytes,
                                              uint32_t ui32PauseCondition,
                                              uint32_t ui32StatusSetClr,
                                              am_hal_mspi_callback_t pfnCallback,
                                              void *pCallbackCtxt)
{
  uint32_t                      ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  ui32Status = psram_nonblocking_transfer(pPsram, false, true,
                                          puiTxBuffer,
                                          ui32WriteAddress,
                                          ui32NumBytes,
                                          ui32PauseCondition,
                                          ui32StatusSetClr,
                                          pfnCallback,
                                          pCallbackCtxt);
  //
  // Check the transaction status.
  //
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  //
  // Return the status.
  //
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Programs the contents of the external psram into a buffer
// with high priority
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_write_hiprio(void *pHandle,
                                                 uint8_t *pui8TxBuffer,
                                                 uint32_t ui32WriteAddress,
                                                 uint32_t ui32NumBytes,
                                                 am_hal_mspi_callback_t pfnCallback,
                                                 void *pCallbackCtxt)
{
  uint32_t                      ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  //
  // Check the transaction status.
  //
  ui32Status = psram_nonblocking_transfer(pPsram, true, true,
                                          pui8TxBuffer,
                                          ui32WriteAddress,
                                          ui32NumBytes,
                                          0,
                                          0,
                                          pfnCallback,
                                          pCallbackCtxt);
  //
  // Check the transaction status.
  //
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  //
  // Return the status.
  //
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief
//!
//! @param pHandle
//! @param pui8TxBuffer
//! @param ui32WriteAddress
//! @param ui32NumBytes
//! @param pfnCallback
//! @param pCallbackCtxt
//!
//! @return
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_nonblocking_write(void *pHandle,
                                                      uint8_t *pui8TxBuffer,
                                                      uint32_t ui32WriteAddress,
                                                      uint32_t ui32NumBytes,
                                                      am_hal_mspi_callback_t pfnCallback,
                                                      void *pCallbackCtxt)
{
  uint32_t                      ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  //
  // Check the transaction status.
  //
  ui32Status = psram_nonblocking_transfer(pPsram, false, true,
                                          pui8TxBuffer,
                                          ui32WriteAddress,
                                          ui32NumBytes,
                                          0,
                                          0,
                                          pfnCallback,
                                          pCallbackCtxt);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  //
  // Return the status.
  //
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Reconfigure MSPI XIP settings.
//
//*****************************************************************************
uint32_t am_devices_mspi_psram_aps12804o_xip_config(void *pHandle, am_hal_mspi_xip_config_t *pXipconfig)
{
    am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;

    if ( pHandle == NULL )
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    am_hal_mspi_xip_config_t *pXipCfg = pPsram->pXipCfg;

    *pXipCfg = *pXipconfig;

    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_XIP_CONFIG, pXipCfg))
    {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
//  Sets up the MSPI and external psram into XIP mode.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_enable_xip(void *pHandle)
{
  uint32_t ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  //
  // Enable XIP on the MSPI.
  //
  ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_XIP_EN, NULL);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
//   Removes the MSPI and external psram from XIP mode.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_disable_xip(void *pHandle)
{
  uint32_t ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  //
  // Disable XIP on the MSPI.
  //
  ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_XIP_DIS, NULL);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
//   Sets up the MSPI and external psram into scrambling mode.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_enable_scrambling(void *pHandle)
{
  uint32_t ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  //
  // Enable scrambling on the MSPI.
  //
  ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_SCRAMB_EN, NULL);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
//   Removes the MSPI and external psram from scrambling mode.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_disable_scrambling(void *pHandle)
{
  uint32_t ui32Status;
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  //
  // Disable Scrambling on the MSPI.
  //
  ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_SCRAMB_DIS, NULL);
  if (AM_HAL_STATUS_SUCCESS != ui32Status)
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Reset the external psram
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_reset(void *pHandle)
{
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  return am_devices_mspi_psram_aps12804o_reset(pPsram);
}

//*****************************************************************************
//
//   Reads the ID of the external psram and returns the value. If device not
//   initialized then return 0xFFFFFFFF
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_id(void *pHandle)
{
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }
  uint32_t ui32DeviceID = 0;
  if ( pPsram->pMspiHandle == NULL || !pPsram->bOccupied )
  {
    return 0xFFFFFFFF;
  }

  return ui32DeviceID;
}

//*****************************************************************************
//
//   Reads the info of the external psram and returns the value.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_info(void *pHandle, am_devices_mspi_psram_info_t *pPsramInfo)
{
  am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
  if ( pHandle == NULL )
  {
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  if ( pPsram->pMspiHandle == NULL || !pPsram->bOccupied )
  {
    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
  }

  pPsramInfo->ui8VendorId = 0xD;
  pPsramInfo->ui8DeviceId = 0x0;
  pPsramInfo->ui32BaseAddr = pPsram->pXipCfg->ui32APBaseAddr;
  pPsramInfo->ui32DeviceSizeKb = 128 / 8 * 1024U;

  return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

#if defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO5_API)
//*****************************************************************************
//
//! @brief write and read back check.
//!
//! @param psMSPISettings - MSPI device structure describing the target spi psram.
//! @param pHandle - MSPI handler which needs to be return
//!
//! This function should be called before any other am_devices_mspi_psram
//! functions. It is used to set tell the other functions how to communicate
//! with the external psram hardware.
//!
//! @return status.
//
//*****************************************************************************
#define PSRAM_CHECK_DATA_SIZE_BYTES  256
//*****************************************************************************
//
//! @brief
//!
//! @param pattern_index
//! @param buff
//! @param len
//!
//! @return
//
//*****************************************************************************
static int prepare_test_pattern(uint32_t pattern_index, uint8_t* buff, uint32_t len)
{
    uint32_t *pui32TxPtr = (uint32_t*)buff;
    uint8_t  *pui8TxPtr  = (uint8_t*)buff;
    //
    // length has to be multiple of 4 bytes
    //
    if ( len % 4 )
    {
        return -1;
    }

    switch ( pattern_index )
    {
        case 0:
            //
            // 0x5555AAAA
            //
            for (uint32_t i = 0; i < len / 4; i++)
            {
               pui32TxPtr[i] = (0x5555AAAA);
            }
            break;
        case 1:
            //
            // 0xFFFF0000
            //
            for (uint32_t i = 0; i < len / 4; i++)
            {
               pui32TxPtr[i] = (0xFFFF0000);
            }
            break;
        case 2:
            //
            // walking
            //
            for (uint32_t i = 0; i < len; i++)
            {
               pui8TxPtr[i] = 0x01 << (i % 8);
            }
            break;
        case 3:
            //
            // incremental from 1
            //
            for (uint32_t i = 0; i < len; i++)
            {
               pui8TxPtr[i] = ((i + 1) & 0xFF);
            }
            break;
        case 4:
            //
            // decremental from 0xff
            //
            for ( uint32_t i = 0; i < len; i++ )
            {
                //
                // decrement starting from 0xff
                //
                pui8TxPtr[i] = (0xff - i) & 0xFF;
            }
            break;
        default:
            //
            // incremental from 1
            //
            for (uint32_t i = 0; i < len; i++)
            {
               pui8TxPtr[i] = ((i + 1) & 0xFF);
            }
            break;

    }

    return 0;
}

//#define MEMORY_WORD_ACCESS
//#define MEMORY_SHORT_ACCESS
//#define MEMORY_BYTE_ACCESS
#define MEMORY_COPY_ACCESS
#if defined(MEMORY_WORD_ACCESS)
AM_SHARED_RW static uint8_t ui32TxBuffer[PSRAM_CHECK_DATA_SIZE_BYTES / 4];
AM_SHARED_RW static uint8_t ui32RxBuffer[PSRAM_CHECK_DATA_SIZE_BYTES / 4];
#elif defined(MEMORY_SHORT_ACCESS)
AM_SHARED_RW static uint8_t ui16TxBuffer[PSRAM_CHECK_DATA_SIZE_BYTES / 2];
AM_SHARED_RW static uint8_t ui16RxBuffer[PSRAM_CHECK_DATA_SIZE_BYTES / 2];
#else
AM_SHARED_RW static uint8_t ui8TxBuffer[PSRAM_CHECK_DATA_SIZE_BYTES];
AM_SHARED_RW static uint8_t ui8RxBuffer[PSRAM_CHECK_DATA_SIZE_BYTES];
#endif

//*****************************************************************************
//
//! @brief
//!
//! @param length
//! @param address
//!
//! @return
//
//*****************************************************************************
static bool
psram_check_by_xip(uint32_t length, uint32_t address)
{
    //
    // Try to use as less ram as possible in stack
    //
    uint32_t ui32NumberOfBytesLeft = length;
    uint32_t ui32TestBytes = 0;
    uint32_t ui32AddressOffset = 0;
    uint8_t ui8PatternCounter = 0;
    uint32_t ix;

    while ( ui32NumberOfBytesLeft )
    {
        if ( ui32NumberOfBytesLeft > PSRAM_CHECK_DATA_SIZE_BYTES )
        {
            ui32TestBytes = PSRAM_CHECK_DATA_SIZE_BYTES;
            ui32NumberOfBytesLeft -= PSRAM_CHECK_DATA_SIZE_BYTES;
        }
        else
        {
            ui32TestBytes = ui32NumberOfBytesLeft;
            ui32NumberOfBytesLeft = 0;
        }

#if defined(MEMORY_WORD_ACCESS)

        //
        // Write to target address with test pattern with given length
        // Use 5 patterns: 0x5555AAAA, 0xFFFF0000, Walking, incremental and decremental
        //
        prepare_test_pattern((ui8PatternCounter) % 5, (uint8_t*)ui32TxBuffer, ui32TestBytes);
        ui8PatternCounter++;
        // write to target address
        am_hal_sysctrl_bus_write_flush();
        uint32_t * pu32Ptr = (uint32_t *)(address + ui32AddressOffset);
        for (ix = 0; ix < ui32TestBytes / 4; ix++)
        {
          *pu32Ptr++ = ui32TxBuffer[ix];
        }

        //
        // Read back data
        //
        am_hal_sysctrl_bus_write_flush();
        pu32Ptr = (uint32_t *)(address + ui32AddressOffset);
        for (ix = 0; ix < ui32TestBytes / 4; ix++)
        {
           ui32RxBuffer[ix] = *pu32Ptr++ ;
        }
        //
        // Verify the result
        //
        for (ix = 0; ix < ui32TestBytes / 4; ix++)
        {
            if (ui32RxBuffer[ix] != ui32TxBuffer[ix])
            {
                //am_util_debug_printf("    Failed to verify at offset 0x%08x, expect data : 0x%08x, read data : 0x%08x !\n", ui32AddressOffset, ui8RxBuffer[ix], ui8TxBuffer[ix]);
                //
                // verify failed, return directly
                //
                return false;

            }
        }

#elif defined(MEMORY_SHORT_ACCESS)

        //
        // Write to target address with test pattern with given length
        // Use 5 patterns: 0x5555AAAA, 0xFFFF0000, Walking, incremental and decremental
        //
        prepare_test_pattern((ui8PatternCounter) % 5, (uint8_t*)ui16TxBuffer, ui32TestBytes);
        ui8PatternCounter++;
        //
        // write to target address
        //
        am_hal_sysctrl_bus_write_flush();
        uint16_t * pu16Ptr = (uint16_t *)(address + ui32AddressOffset);
        for (ix = 0; ix < ui32TestBytes / 2; ix++)
        {
          *pu16Ptr++ = ui16TxBuffer[ix];
        }

        //
        // Read back data
        //
        am_hal_sysctrl_bus_write_flush();
        pu16Ptr = (uint16_t *)(address + ui32AddressOffset);
        for (ix = 0; ix < ui32TestBytes / 2; ix++)
        {
           ui16RxBuffer[ix] = *pu16Ptr++ ;
        }
        //
        // Verify the result
        //
        for (ix = 0; ix < ui32TestBytes / 2; ix++)
        {
            if (ui16RxBuffer[ix] != ui16TxBuffer[ix])
            {
                //am_util_debug_printf("    Failed to verify at offset 0x%08x, expect data : 0x%08x, read data : 0x%08x !\n", ui32AddressOffset, ui8RxBuffer[ix], ui8TxBuffer[ix]);
                //
                // verify failed, return directly
                //
                return false;

            }
        }
#elif defined(MEMORY_BYTE_ACCESS)

        //
        // Write to target address with test pattern with given length
        // Use 5 patterns: 0x5555AAAA, 0xFFFF0000, Walking, incremental and decremental
        //

        prepare_test_pattern((ui8PatternCounter) % 5, ui8TxBuffer, ui32TestBytes);
        ui8PatternCounter++;
        //
        // write to target address
        //
        am_hal_sysctrl_bus_write_flush();
        uint8_t * pu8Ptr = (uint8_t *)(address + ui32AddressOffset);
        for (ix = 0; ix < ui32TestBytes; ix++)
        {
          *pu8Ptr++ = ui8TxBuffer[ix];
        }

        //
        // Read back data
        //
        am_hal_sysctrl_bus_write_flush();
        pu8Ptr = (uint8_t *)(address + ui32AddressOffset);
        for (ix = 0; ix < ui32TestBytes; ix++)
        {
          ui8RxBuffer[ix] = *pu8Ptr++ ;
        }

        //
        // Verify the result
        //
        for (ix = 0; ix < ui32TestBytes; ix++)
        {
            if (ui8RxBuffer[ix] != ui8TxBuffer[ix])
            {
                //am_util_debug_printf("    Failed to verify at offset 0x%08x, expect data : 0x%08x, read data : 0x%08x !\n", ui32AddressOffset, ui8RxBuffer[ix], ui8TxBuffer[ix]);
                //
                // verify failed, return directly
                //
                return false;

            }

        }
#elif defined(MEMORY_COPY_ACCESS)

        //
        // Write to target address with test pattern with given length
        // Use 5 patterns: 0x5555AAAA, 0xFFFF0000, Walking, incremental and decremental
        //

        prepare_test_pattern((ui8PatternCounter) % 5, ui8TxBuffer, ui32TestBytes);
        ui8PatternCounter++;
        //
        // write to target address
        //
        am_hal_sysctrl_bus_write_flush();
        uint8_t * xipPointer = (uint8_t *)(address + ui32AddressOffset);
        memcpy(xipPointer, (uint8_t*)ui8TxBuffer, ui32TestBytes);

        //
        // Read back data
        //
        am_hal_sysctrl_bus_write_flush();
        xipPointer = (uint8_t *)(address + ui32AddressOffset);
        memcpy((uint8_t*)ui8RxBuffer, xipPointer, ui32TestBytes);
        //
        // Verify the result
        //
        for (ix = 0; ix < ui32TestBytes; ix++)
        {
            if (ui8RxBuffer[ix] != ui8TxBuffer[ix])
            {
                //am_util_debug_printf("    Failed to verify at offset 0x%08x, expect data : 0x%08x, read data : 0x%08x !\n", ui32AddressOffset, ui8RxBuffer[ix], ui8TxBuffer[ix]);
                //
                // verify failed, return directly
                //
                return false;

            }

        }
#endif

        ui32AddressOffset += ui32TestBytes;
    }

    return true;
}

//*****************************************************************************
//
//! @brief Count the longest consecutive 1s in a 32bit word
//! @details Static helper function:
//!
//! @param pVal
//!
//! @return
//
//*****************************************************************************
static uint32_t
count_consecutive_ones(uint32_t* pVal)
{
    uint32_t count = 0;
    uint32_t data = *pVal;

    while ( data )
    {
        data = (data & (data << 1));
        count++;
    }
    return count;
}

//*****************************************************************************
//
//! @brief Find and return the mid point of the longest continuous 1s in a 32bit word
//! @details Static helper function:
//!
//! @param pVal
//!
//! @return
//
//*****************************************************************************
static uint32_t
find_mid_point(uint32_t* pVal)
{
    uint32_t pattern_len = 0;
    uint32_t max_len = 0;
    uint32_t pick_point = 0;
    bool pattern_start = false;
    uint32_t val = *pVal;
    uint8_t remainder = 0;
    bool pick_point_flag = false;

    for ( uint32_t i = 0; i < 32; i++ )
    {
        if ( val & (0x01 << i) )
        {
            pattern_start = true;
            pattern_len++;
        }
        else
        {
            if ( pattern_start == true )
            {
                pattern_start = false;
                pick_point_flag = true;
            }
        }
        if ( (i == 31) && ( pattern_start == true ) )
        {
            pick_point_flag = true;
        }

        if (pick_point_flag == true)
        {
            if ( pattern_len > max_len )
            {
                max_len = pattern_len;
                pick_point = i - 1 - pattern_len / 2;
                remainder = pattern_len % 2;
            }
            pattern_len = 0;
            pick_point_flag = false;
        }
    }

    //
    // check the passing window side
    //
// #### INTERNAL BEGIN ####
//
// HSP20-339: Try to locate and move the middle value found one bit further away from the failure window.
// In our current tests, most of the time, we see the failure window is in the middle of 1-30 range.
// Passing windows are likely to be on either end of the 1-30 range.
// Therefore, adding comparison here, if we see the middle point is below 16, it is likely that we found
// a passing window starting from 1 at the lower side, if it is true (setting 1 passed),
// we further move the middle value to the lower side by 1.
// And the same for the higher values.
//
// #### INTERNAL END ####

    if ( (pick_point < 16) && (val & 0x00000002) )
    {
        //
        // window is likely on low side
        //
        pick_point = pick_point - remainder;    // minus only when pattern length is odd
    }
    else if ( (pick_point > 15) && (val & 0x40000000) )
    {
        //
        // window is likely on high side
        //
        pick_point = pick_point + 1;
    }
    else
    {
        //
        // window is in the middle, no action
        //
    }

    return pick_point;
}

//*****************************************************************************
//
//  Checks PSRAM timing and determine a delay setting.
//
//*****************************************************************************
#if defined(AM_PART_APOLLO5_API)
// #### INTERNAL BEGIN ####
// CAYNSWS-1239 reduce timing scan time
// #### INTERNAL END ####
#define PSRAM_TIMING_SCAN_SIZE_BYTES (4*AM_DEVICES_MSPI_PSRAM_PAGE_SIZE)
#else
#define PSRAM_TIMING_SCAN_SIZE_BYTES (128*AM_DEVICES_MSPI_PSRAM_PAGE_SIZE)
#endif

#if defined(AM_PART_APOLLO4P)
#define SCAN_TXDQSDELAY
#define SCAN_RXDQSDELAY
#elif defined(AM_PART_APOLLO4L)
#define SCAN_RXDQSDELAY
#elif defined(AM_PART_APOLLO330P_510L)
#define SCAN_RXDQSDELAY
#else
#define SCAN_TXDQSDELAY
#define SCAN_RXDQSDELAY
#endif

#if defined(SCAN_TXNEG)
#define SCAN_TXNEG_START 0
#define SCAN_TXNEG_END   1
#endif

#if defined(SCAN_RXNEG)
#define SCAN_RXNEG_START 0
#define SCAN_RXNEG_END   1
#endif

#if defined(SCAN_RXCAP)
#define SCAN_RXCAP_START 0
#define SCAN_RXCAP_END   1
#endif

#if defined(SCAN_TURNAROUND)
#define SCAN_TURNAROUND_START 0
#define SCAN_TURNAROUND_END   1
#endif

#if defined(SCAN_TXDQSDELAY)
#define SCAN_TXDQSDELAY_START 0
#define SCAN_TXDQSDELAY_END   31
#endif

#if defined(SCAN_RXDQSDELAY)
#define SCAN_RXDQSDELAY_START 0
#define SCAN_RXDQSDELAY_END   31
#endif

//*****************************************************************************
//
// Checks PSRAM timing and determine a delay setting.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_ddr_init_timing_check(uint32_t ui32Module,
                                                      am_devices_mspi_psram_config_t *pDevCfg,
                                                      am_devices_mspi_psram_ddr_timing_config_t *pDevDdrCfg)
{
    uint32_t ui32Status;
    am_devices_mspi_psram_t *pPsram;
    void *pHandle;
    uint32_t Txdqsdelay = 0;
    uint32_t Rxdqsdelay = 0;

    uint32_t ui32CheckAddress;
    uint32_t ui32CCOResult = 0;
    uint32_t ui32TxResult = 0;
    uint32_t ui32RxResultArray[32];

    am_devices_mspi_psram_ddr_timing_config_t *pTimingCfg;
    am_hal_mspi_timing_scan_t scanCfg;
    //
    // initialize interface
    //
    ui32Status = am_devices_mspi_psram_aps12804o_ddr_init(ui32Module, pDevCfg, (void **)&pPsram, &pHandle);
    if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("    Failed to configure the MSPI and PSRAM Device correctly!\n");
        return ui32Status;
    }

    //
    //get value configured by init & hal
    //
    ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_TIMING_SCAN_GET, &scanCfg);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Put the MSPI into XIP mode.
    //
    ui32Status = am_devices_mspi_psram_aps12804o_ddr_enable_xip(pPsram);
    if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("    Failed to disable XIP mode in the MSPI!\n");
        return ui32Status;
    }

    pTimingCfg = pPsram->pTimingCfg;
#if defined(FAST_TIMING_SCAN)
    if ( pTimingCfg->bValid )
    {
        //
        // apply settings
        //
        ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_TIMING_SCAN, &pTimingCfg->sTimingCfg);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
        }
        //
        // run data check
        //
        if ( psram_check_by_xip(PSRAM_TIMING_SCAN_SIZE_BYTES, pPsram->pXipCfg->ui32APBaseAddr) )
        {
          *pDevDdrCfg = *pTimingCfg;
          am_devices_mspi_psram_aps12804o_ddr_deinit(pPsram);
          am_util_debug_printf("    Skipping Timing Scan!\n");
          return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
        }
    }
#endif

    am_util_debug_printf("    Start Use XIP to Timing Scan!\n");
    uint32_t ui32TxNeg = scanCfg.bTxNeg;
    uint32_t ui32RxNeg = scanCfg.bRxNeg;
    uint32_t ui32RxCap = scanCfg.bRxCap;
    uint32_t ui32Turnaround = scanCfg.ui8Turnaround;
#if defined(SCAN_TXNEG)
    for ( ui32TxNeg = SCAN_TXNEG_START; ui32TxNeg <= SCAN_TXNEG_END; ui32TxNeg++ )
#endif
    {
        scanCfg.bTxNeg = (bool)ui32TxNeg;
#if defined(SCAN_RXNEG)
        for ( ui32RxNeg = SCAN_RXNEG_START; ui32RxNeg <= SCAN_RXNEG_END; ui32RxNeg++ )
#endif
        {
            scanCfg.bRxNeg = (bool)ui32RxNeg;
#if defined(SCAN_RXCAP)
            for ( ui32RxCap = SCAN_RXCAP_START; ui32RxCap <= SCAN_RXCAP_END; ui32RxCap++ )
#endif
            {
                scanCfg.bRxCap = (bool)ui32RxCap;
#if defined(SCAN_TURNAROUND)
                for (scanCfg.ui8Turnaround = ui32Turnaround + SCAN_TURNAROUND_START; scanCfg.ui8Turnaround <= ui32Turnaround + SCAN_TURNAROUND_END; scanCfg.ui8Turnaround++ )
#endif
                {
                    am_util_debug_printf("    TxNeg=%d, RxNeg=%d, RxCap=%d, Turnaround=%d\n", scanCfg.bTxNeg, scanCfg.bRxNeg, scanCfg.bRxCap, scanCfg.ui8Turnaround)
                    ui32TxResult = 0;
                    memset(ui32RxResultArray, 0, sizeof(ui32RxResultArray));
#if defined(SCAN_TXDQSDELAY)
                    for (scanCfg.ui8TxDQSDelay = SCAN_TXDQSDELAY_START; scanCfg.ui8TxDQSDelay <= SCAN_TXDQSDELAY_END; scanCfg.ui8TxDQSDelay++)
#endif
                    {
#if defined(SCAN_RXDQSDELAY)
                        for (scanCfg.ui8RxDQSDelay = SCAN_RXDQSDELAY_START; scanCfg.ui8RxDQSDelay <= SCAN_RXDQSDELAY_END; scanCfg.ui8RxDQSDelay++)
#endif
                        {
                            //
                            // apply settings
                            //
                            ui32Status = am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_TIMING_SCAN, &scanCfg);
                            if (AM_HAL_STATUS_SUCCESS != ui32Status)
                            {
                                return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
                            }
                            uint32_t ui32Offset = (scanCfg.bTxNeg + scanCfg.bRxNeg + scanCfg.bRxCap + scanCfg.ui8Turnaround) * PSRAM_TIMING_SCAN_SIZE_BYTES + (scanCfg.ui8TxDQSDelay + scanCfg.ui8RxDQSDelay) * 2;
                            ui32CheckAddress = pPsram->pXipCfg->ui32APBaseAddr + ui32Offset;
                            //
                            // run data check
                            //
                            if ( psram_check_by_xip(PSRAM_TIMING_SCAN_SIZE_BYTES, ui32CheckAddress) )
                            {
                                //
                                // data check pass
                                //
                                ui32RxResultArray[scanCfg.ui8TxDQSDelay] |= 0x01 << scanCfg.ui8RxDQSDelay;
                            }
                            else
                            {
                                //
                                // data check failed
                                //
                            }
                        }
#if defined(SCAN_RXDQSDELAY)
                        ui32CCOResult = count_consecutive_ones(&ui32RxResultArray[scanCfg.ui8TxDQSDelay]);
                        if ( ui32CCOResult > PSRAM_TIMING_SCAN_MIN_ACCEPTANCE_LENGTH )
                        {
                            ui32TxResult |= 0x01 << scanCfg.ui8TxDQSDelay;
                        }
                        am_util_debug_printf("    TxDQSDelay: %d, RxDQSDelay Scan = 0x%08X, Window size = %d\n", scanCfg.ui8TxDQSDelay, ui32RxResultArray[scanCfg.ui8TxDQSDelay], ui32CCOResult);
#else
                        if ( ui32RxResultArray[scanCfg.ui8TxDQSDelay] != 0 )
                        {
                            ui32TxResult |= 0x01 << scanCfg.ui8TxDQSDelay;
                        }
                        am_util_debug_printf("    TxDQSDelay: %d, RxDQSDelay Scan = 0x%08X\n", scanCfg.ui8TxDQSDelay, ui32RxResultArray[scanCfg.ui8TxDQSDelay]);
#endif
                    }
                    //
                    // Check Result
                    //
                    if ( ui32TxResult == 0 )
                    {
                        //
                        // no window is found
                        //
#if defined(SCAN_TXNEG) || defined(SCAN_RXNEG) || defined(SCAN_RXCAP) || defined(SCAN_TURNAROUND)
                        continue;
#else
                        am_util_debug_printf("Timing Scan found no window!\n");
                        //
                        // Deinitialize the MSPI interface
                        //
                        am_devices_mspi_psram_aps12804o_ddr_deinit(pPsram);
                        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
#endif
                    }
#if defined(SCAN_TXDQSDELAY)
                    //
                    // Find TXDQSDELAY Value
                    //
                    Txdqsdelay = find_mid_point(&ui32TxResult);
#else
                    Txdqsdelay = scanCfg.ui8TxDQSDelay;
#endif

#if defined(SCAN_RXDQSDELAY)
                    //
                    // Find RXDQSDELAY Value
                    //
                    Rxdqsdelay = find_mid_point(&ui32RxResultArray[Txdqsdelay]);
#else
                    Rxdqsdelay = scanCfg.ui8RxDQSDelay;
#endif

                    am_util_debug_printf("Selected timing scan setting: TxNeg=%d, RxNeg=%d, RxCap=%d, Turnaround=%d, TxDQSDelay=%d, RxDQSDelay=%d\n", scanCfg.bTxNeg, scanCfg.bRxNeg, scanCfg.bRxCap, scanCfg.ui8Turnaround, Txdqsdelay, Rxdqsdelay);

                    scanCfg.ui8TxDQSDelay = Txdqsdelay;
                    scanCfg.ui8RxDQSDelay = Rxdqsdelay;
                    pTimingCfg->sTimingCfg = scanCfg;
                    pTimingCfg->bValid = true;
                    //
                    // Set output values
                    //
                    *pDevDdrCfg = *pTimingCfg;
                    am_devices_mspi_psram_aps12804o_ddr_deinit(pPsram);
                    return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
                }
            }
        }
    }
    am_util_debug_printf("Timing Scan found no window!\n");
    //
    // Deinitialize the MSPI interface
    //
    am_devices_mspi_psram_aps12804o_ddr_deinit(pPsram);

    return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
}

//*****************************************************************************
//
//  Apply given DDR timing settings to target MSPI instance.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_apply_ddr_timing(void *pHandle,
                                                 am_devices_mspi_psram_ddr_timing_config_t *pDevDdrCfg)
{
    am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;
    if ( pHandle == NULL )
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
    //
    // apply timing settings
    //
    *pPsram->pTimingCfg = *pDevDdrCfg;
    am_util_stdio_printf("    Apply Timing TxNeg=%d, RxNeg=%d, RxCap=%d, Turnaround=%d, TxDQSDelay=%d, RxDQSDelay=%d\n", pDevDdrCfg->sTimingCfg.bTxNeg,
                                                                                                                         pDevDdrCfg->sTimingCfg.bRxNeg,
                                                                                                                         pDevDdrCfg->sTimingCfg.bRxCap,
                                                                                                                         pDevDdrCfg->sTimingCfg.ui8Turnaround,
                                                                                                                         pDevDdrCfg->sTimingCfg.ui8TxDQSDelay,
                                                                                                                         pDevDdrCfg->sTimingCfg.ui8RxDQSDelay);
    return am_hal_mspi_control(pPsram->pMspiHandle, AM_HAL_MSPI_REQ_TIMING_SCAN, &pPsram->pTimingCfg->sTimingCfg);

}
#endif

//*****************************************************************************
//
// Enter half sleep
//
// Send a command to Enter Half Sleep Mode. Will need to Be in OCTAL mode
// to access the register per the device driver.
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_enter_halfsleep(void *pHandle)
{
    am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;

    if ( pHandle == NULL )
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    am_hal_mspi_dev_config_t stSetting = pPsram->stSetting;
    //
    // Disable MSPI defore re-configuring it
    //
    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_disable(pPsram->pMspiHandle))
    {
        am_util_debug_printf("Error - Failed to Disable MSPI.\n");
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // maintain 24MHz clock so that when exiting halfsleep, the dummy command CE low pulse is fixed.
    //
    stSetting.eClockFreq = MSPI_BASE_FREQUENCY;

    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_device_configure(pPsram->pMspiHandle, &stSetting))
    {
        am_util_debug_printf("Error - Failed to configure MSPI device.\n");
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Re-Enable MSPI
    //
    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_enable(pPsram->pMspiHandle))
    {
        am_util_debug_printf("Error - Failed to Enable MSPI!\n");
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Re-config the MSPI pins.
    //
    am_bsp_mspi_pins_enable(pPsram->ui32Module, stSetting.eDeviceConfig);

    //
    // Send command to Enter half sleep
    //
    if (AM_HAL_STATUS_SUCCESS != am_device_command_write(pPsram->pMspiHandle, AM_DEVICES_MSPI_PSRAM_DDR_HALFSLEEP_ENTRY, false, 0, NULL, 0))
    {
        am_util_debug_printf("Failed to write PSRAM MR6 register!\n");
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    am_util_delay_us(APS12804O_tHS_MIN_US);

    return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

//*****************************************************************************
//
// This function resets the device to bring it out of halfsleep
//
//*****************************************************************************
uint32_t
am_devices_mspi_psram_aps12804o_exit_halfsleep(void *pHandle)
{
    am_devices_mspi_psram_t *pPsram = (am_devices_mspi_psram_t *)pHandle;

    if ( pHandle == NULL )
    {
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
#if !defined(AM_PART_APOLLO5_API)
    uint32_t pin_num = 0;
    am_hal_gpio_pincfg_t gpio_pincfg = AM_HAL_GPIO_PINCFG_DEFAULT;
    am_bsp_mspi_ce_pincfg_get(pPsram->ui32Module, pPsram->eDeviceConfig, &pin_num, &gpio_pincfg);

    //
    // Configure CE pin to output and hold high
    //
    am_hal_gpio_output_set(pin_num);
    am_hal_gpio_pinconfig(pin_num, am_hal_gpio_pincfg_output);

    //
    // Start reset pulse on CE1
    //
    am_hal_gpio_output_clear(pin_num);

    //
    // hold reset pin for 60ns - 500ns
    //
    APS12804O_tXPHS_delay(1);

    //
    // Set pin to high to finish reset
    //
    am_hal_gpio_output_set(pin_num);

    //
    // Reconfigure pin for CE on PSRAM
    //
    am_hal_gpio_pinconfig(pin_num, gpio_pincfg);
#else
    uint32_t ui32PIOBuffer = 0;
    //
    // Send dummy command to pull CE for tXPHS
    //
    if (AM_HAL_STATUS_SUCCESS != am_device_command_write(pPsram->pMspiHandle, 0x0000, true, 0, &ui32PIOBuffer, 2))
    {
      am_util_debug_printf("Error - Failed to send dummy command.\n");
      return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }
#endif
    //
    // Delay after setting pin high to allow for device to accept command
    //  and go into half sleep mode
    //
    am_util_delay_us(APS12804O_tXHS_MIN_US);

    //
    // Disable MSPI defore re-configuring it
    //
    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_disable(pPsram->pMspiHandle))
    {
        am_util_debug_printf("Error - Failed to Disable MSPI.\n");
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Reconfigure MSPI
    //
    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_device_configure(pPsram->pMspiHandle, &pPsram->stSetting))
    {
        am_util_debug_printf("Error - Failed to configure MSPI device.\n");
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Re-Enable MSPI
    //
    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_enable(pPsram->pMspiHandle))
    {
        am_util_debug_printf("Error - Failed to Enable MSPI!\n");
        return AM_DEVICES_MSPI_PSRAM_STATUS_ERROR;
    }

    //
    // Re-config the MSPI pins.
    //
    am_bsp_mspi_pins_enable(pPsram->ui32Module, pPsram->stSetting.eDeviceConfig);

    return AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS;
}

#if (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION < 6000000)
__asm void
APS12804O_tXPHS_delay( uint32_t ui32Iterations )
{
    subs    r0, #1
    bne     APS12804O_tXPHS_delay
    bx      lr
}
#elif (defined (__ARMCC_VERSION)) && (__ARMCC_VERSION >= 6000000)
void
APS12804O_tXPHS_delay( uint32_t ui32Iterations )
{
  __asm
  (
    " subs  r0, #1\n"
    " bne   APS12804O_tXPHS_delay\n"
  );
}
#elif defined(__GNUC_STDC_INLINE__)
__attribute__((naked))
void
APS12804O_tXPHS_delay( uint32_t ui32Iterations )
{
    __asm
    (
        "   subs    r0, #1\n"
        "   bne     APS12804O_tXPHS_delay\n"
        "   bx      lr\n"
    );
}
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma diag_suppress = Pe940   // Suppress IAR compiler warning about missing
                                // return statement on a non-void function
__stackless inline void
APS12804O_tXPHS_delay( uint32_t ui32Iterations )
{
    __asm(" subs    r0, #1 ");
    __asm(" bne     APS12804O_tXPHS_delay ");
}
#pragma diag_default = Pe940    // Restore IAR compiler warning
#else
#error Compiler is unknown, please contact Ambiq support team
#endif

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
