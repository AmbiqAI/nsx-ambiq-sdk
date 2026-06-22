//*****************************************************************************
//
//! @file am_devices_mspi_gd25lb.c
//!
//! @brief General Multibit SPI Flash driver.
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
#include "am_devices_mspi_gd25lb.h"
#include "am_util_stdio.h"
#include "am_bsp.h"
#include "am_util.h"
#include "am_util_delay.h"

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************


#define AM_DEVICES_MSPI_GD25LB512_TIMEOUT             1000000
#define AM_DEVICES_MSPI_GD25LB512_ERASE_TIMEOUT       1000000

#define AM_DEVICES_MSPI_GD25LB512_SECTOR_FOR_TIMING_CHECK     30       // max 31

#define FLASH_TIMING_SCAN_MIN_ACCEPTANCE_LENGTH           (8)     // there should be at least
                                                                      // this amount of consecutive
                                                                      // passing settings to be accepted.
#define FLASH_CHECK_DATA_SIZE_BYTES                         AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE   // Data trunk size
#define FLASH_TIMING_SCAN_SIZE_BYTES                        AM_DEVICES_MSPI_GD25LB512_SECTOR_SIZE // Total scan size
#define FLASH_TEST_PATTERN_NUMBER                           5       // 5 patterns

typedef struct
{
    uint8_t ui8DummyCycle;
    union
    {
        uint8_t ui8OTPConfig;
        struct
        {
            uint8_t secReg          : 1;            //[0..0] Security Registers unlock
            uint8_t                 : 3;            //
            uint8_t SRP1            : 1;            //[4..4] SRP1 unlock
            uint8_t                 : 3;
        }ui8OTPConfig_b;
    };
    uint8_t ui8DriveStrength;
    union
    {
        uint8_t ui8OdtDlpProtect;
        struct
        {
            uint8_t                 : 2;
            uint8_t Protect         : 1;            //[2..2] Protection configuration
            uint8_t DLP             : 1;            //[3..3] data learning pattern
            uint8_t ODT             : 2;            //[4..5] On Die termination
            uint8_t                 : 2;
        }ui8OdtDlpProtect_b;
    };
    uint8_t ui8AddrMode;
    uint8_t ui8XipConfig;
    uint8_t ui8WrapConfig;
} am_devices_mspi_gd25lb512_regcfg_t;

typedef struct
{
    uint32_t                             ui32Module;
    void                                 *pMspiHandle;
    am_devices_mspi_gd25lb512_regcfg_t   regCfg;
    am_hal_mspi_dev_config_t             currDevCfg;
    am_hal_mspi_dev_config_t             cmdDevCfg;
    bool                                 bOccupied;

    am_hal_mspi_config_t                 *pMspiCfg;      //pointer to global storage
    am_hal_mspi_xip_config_t             *pXipCfg;       //pointer to global storage
    am_hal_mspi_rxcfg_t                  *pRxCfg;        //pointer to global storage
    am_hal_mspi_timing_scan_t            *pTimingCfg;    //pointer to global storage
    bool                                 bTimingValid;   //Are timing scan params valid

} am_devices_mspi_gd25lb512_t;

#if defined(AM_PART_APOLLO4_API) || defined(AM_PART_APOLLO5_API)
static am_hal_mspi_xip_config_t gGDXipConfig[] =
{
    {
        .ui32APBaseAddr       = MSPI0_APERTURE_START_ADDR,
        .eAPMode              = AM_HAL_MSPI_AP_READ_ONLY,
        .eAPSize              = AM_HAL_MSPI_AP_SIZE64M,
        .scramblingStartAddr  = 0,
        .scramblingEndAddr    = 0,
    },
    {
        .ui32APBaseAddr       = MSPI1_APERTURE_START_ADDR,
        .eAPMode              = AM_HAL_MSPI_AP_READ_ONLY,
        .eAPSize              = AM_HAL_MSPI_AP_SIZE64M,
        .scramblingStartAddr  = 0,
        .scramblingEndAddr    = 0,
    },
    {
        .ui32APBaseAddr       = MSPI2_APERTURE_START_ADDR,
        .eAPMode              = AM_HAL_MSPI_AP_READ_ONLY,
        .eAPSize              = AM_HAL_MSPI_AP_SIZE64M,
        .scramblingStartAddr  = 0,
        .scramblingEndAddr    = 0,
    },
#if AM_REG_MSPI_NUM_MODULES == 4
    {
        .ui32APBaseAddr       = MSPI3_APERTURE_START_ADDR,
        .eAPMode              = AM_HAL_MSPI_AP_READ_ONLY,
        .eAPSize              = AM_HAL_MSPI_AP_SIZE64M,
        .scramblingStartAddr  = 0,
        .scramblingEndAddr    = 0,
    },
#endif
};

static am_hal_mspi_rxcfg_t gGDMspiRxCfg =
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

static am_hal_mspi_config_t gGDMspiCfg[] =
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
#if AM_REG_MSPI_NUM_MODULES == 4
  {
    .ui32TCBSize          = 0,
    .pTCB                 = NULL,
    .bClkonD4             = 0
  },
#endif
};

static am_hal_mspi_timing_scan_t gGDTimeCfg[] =
{
    {
// #### INTERNAL BEGIN ####
#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO330P_510L)
        .bTxDataDelay      = false,
#endif
// #### INTERNAL END ####
        .bTxNeg            = 1,
        .bRxNeg            = 0,
        .bRxCap            = 0,
        .ui8TxDQSDelay     = 4,
        .ui8RxDQSDelay     = 16,
        .ui8Turnaround     = 8,
    },
    {
// #### INTERNAL BEGIN ####
#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO330P_510L)
        .bTxDataDelay      = false,
#endif
// #### INTERNAL END ####
        .bTxNeg            = 1,
        .bRxNeg            = 0,
        .bRxCap            = 0,
        .ui8TxDQSDelay     = 4,
        .ui8RxDQSDelay     = 16,
        .ui8Turnaround     = 8,
    },
    {
// #### INTERNAL BEGIN ####
#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO330P_510L)
        .bTxDataDelay      = false,
#endif
// #### INTERNAL END ####
        .bTxNeg            = 1,
        .bRxNeg            = 0,
        .bRxCap            = 0,
        .ui8TxDQSDelay     = 4,
        .ui8RxDQSDelay     = 16,
        .ui8Turnaround     = 8,
    },
#if AM_REG_MSPI_NUM_MODULES == 4
    {
// #### INTERNAL BEGIN ####
#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO330P_510L)
        .bTxDataDelay      = false,
#endif
// #### INTERNAL END ####
        .bTxNeg            = 1,
        .bRxNeg            = 0,
        .bRxCap            = 0,
        .ui8TxDQSDelay     = 4,
        .ui8RxDQSDelay     = 16,
        .ui8Turnaround     = 8,
    },
#endif
};

#endif

am_devices_mspi_gd25lb512_t gAmGd25lb[AM_DEVICES_MSPI_GD25LB512_MAX_DEVICE_NUM];

am_hal_mspi_dev_config_t MSPI_GD25LB512_Serial_CE0_MSPIConfig =
{
    .eSpiMode             = AM_HAL_MSPI_SPI_MODE_0,
    .eClockFreq           = AM_HAL_MSPI_CLK_8MHZ,
    .ui8TurnAround        = 8,
    .eAddrCfg             = AM_HAL_MSPI_ADDR_4_BYTE,
    .eInstrCfg            = AM_HAL_MSPI_INSTR_1_BYTE,
    .eDeviceConfig        = AM_HAL_MSPI_FLASH_SERIAL_CE0,
    .bSendInstr           = true,
    .bSendAddr            = true,
    .bTurnaround          = true,

    .ui16ReadInstr         = AM_DEVICES_MSPI_GD25LB512_FAST_READ,
    .ui16WriteInstr        = AM_DEVICES_MSPI_GD25LB512_PAGE_PROGRAM,

#if defined(AM_PART_APOLLO4_API) || defined(AM_PART_APOLLO5_API)
    .ui8WriteLatency      = 0,
    .bEnWriteLatency      = false,
    .bEmulateDDR          = false,
    .ui16DMATimeLimit     = 0,
    .eDMABoundary         = AM_HAL_MSPI_BOUNDARY_NONE,
#if defined(AM_PART_APOLLO4)
    .eDeviceNum           = AM_HAL_MSPI_DEVICE0,
#endif
#else
    .ui32TCBSize          = 0,
    .pTCB                 = NULL,
    .scramblingStartAddr  = 0,
    .scramblingEndAddr    = 0,
#endif
};

am_hal_mspi_dev_config_t MSPI_GD25LB512_Serial_CE1_MSPIConfig =
{
    .eSpiMode             = AM_HAL_MSPI_SPI_MODE_0,
    .eClockFreq           = AM_HAL_MSPI_CLK_8MHZ,
    .ui8TurnAround        = 8,
    .eAddrCfg             = AM_HAL_MSPI_ADDR_4_BYTE,
    .eInstrCfg            = AM_HAL_MSPI_INSTR_1_BYTE,
    .eDeviceConfig        = AM_HAL_MSPI_FLASH_SERIAL_CE1,
    .bSendInstr           = true,
    .bSendAddr            = true,
    .bTurnaround          = true,
    .ui16ReadInstr         = AM_DEVICES_MSPI_GD25LB512_FAST_READ,
    .ui16WriteInstr        = AM_DEVICES_MSPI_GD25LB512_PAGE_PROGRAM,

#if defined(AM_PART_APOLLO4_API) || defined(AM_PART_APOLLO5_API)
    .ui8WriteLatency      = 0,
    .bEnWriteLatency      = false,
    .bEmulateDDR          = false,
    .ui16DMATimeLimit     = 0,
    .eDMABoundary         = AM_HAL_MSPI_BOUNDARY_NONE,
#if defined(AM_PART_APOLLO4)
    .eDeviceNum           = AM_HAL_MSPI_DEVICE0,
#endif
#else
    .ui32TCBSize          = 0,
    .pTCB                 = NULL,
    .scramblingStartAddr  = 0,
    .scramblingEndAddr    = 0,
#endif
};

am_hal_mspi_dev_config_t MSPI_GD25LB512_Quad_CE0_MSPIConfig =
{
    .eSpiMode             = AM_HAL_MSPI_SPI_MODE_0,
    .eClockFreq           = AM_HAL_MSPI_CLK_8MHZ,
    .ui8TurnAround        = 6,
    .eAddrCfg             = AM_HAL_MSPI_ADDR_4_BYTE,
    .eInstrCfg            = AM_HAL_MSPI_INSTR_1_BYTE,
    .eDeviceConfig        = AM_HAL_MSPI_FLASH_QUAD_CE0,
    .bSendInstr           = true,
    .bSendAddr            = true,
    .bTurnaround          = true,
    .ui16ReadInstr         = AM_DEVICES_MSPI_GD25LB512_QUAD_IO_READ,
    .ui16WriteInstr        = AM_DEVICES_MSPI_GD25LB512_QUAD_PAGE_PROGRAM,

#if defined(AM_PART_APOLLO4_API) || defined(AM_PART_APOLLO5_API)
    .ui8WriteLatency      = 0,
    .bEnWriteLatency      = false,
    .bEmulateDDR          = false,
    .ui16DMATimeLimit     = 0,
    .eDMABoundary         = AM_HAL_MSPI_BOUNDARY_NONE,
#if defined(AM_PART_APOLLO4)
    .eDeviceNum           = AM_HAL_MSPI_DEVICE0,
#endif
#else
    .ui32TCBSize          = 0,
    .pTCB                 = NULL,
    .scramblingStartAddr  = 0,
    .scramblingEndAddr    = 0,
#endif
};

am_hal_mspi_dev_config_t MSPI_GD25LB512_Quad_CE1_MSPIConfig =
{
    .eSpiMode             = AM_HAL_MSPI_SPI_MODE_0,
    .eClockFreq           = AM_HAL_MSPI_CLK_8MHZ,
    .ui8TurnAround        = 6,
    .eAddrCfg             = AM_HAL_MSPI_ADDR_4_BYTE,
    .eInstrCfg            = AM_HAL_MSPI_INSTR_1_BYTE,
    .eDeviceConfig        = AM_HAL_MSPI_FLASH_QUAD_CE1,
    .bSendInstr           = true,
    .bSendAddr            = true,
    .bTurnaround          = true,
    .ui16ReadInstr         = AM_DEVICES_MSPI_GD25LB512_QUAD_IO_READ,
    .ui16WriteInstr        = AM_DEVICES_MSPI_GD25LB512_QUAD_PAGE_PROGRAM,

#if defined(AM_PART_APOLLO4_API) || defined(AM_PART_APOLLO5_API)
    .ui8WriteLatency      = 0,
    .bEnWriteLatency      = false,
    .bEmulateDDR          = false,
    .ui16DMATimeLimit     = 0,
    .eDMABoundary         = AM_HAL_MSPI_BOUNDARY_NONE,
#if defined(AM_PART_APOLLO4)
    .eDeviceNum           = AM_HAL_MSPI_DEVICE0,
#endif
#else
    .ui32TCBSize          = 0,
    .pTCB                 = NULL,
    .scramblingStartAddr  = 0,
    .scramblingEndAddr    = 0,
#endif
};

struct
{
    am_hal_mspi_device_e eHalDeviceEnum;
    am_hal_mspi_dev_config_t *psDevConfig;
} g_GD25LB512_DevConfig[] =
{
    {AM_HAL_MSPI_FLASH_SERIAL_CE0,              &MSPI_GD25LB512_Serial_CE0_MSPIConfig},
    {AM_HAL_MSPI_FLASH_SERIAL_CE1,              &MSPI_GD25LB512_Serial_CE1_MSPIConfig},
    {AM_HAL_MSPI_FLASH_QUAD_CE0,                &MSPI_GD25LB512_Quad_CE0_MSPIConfig},
    {AM_HAL_MSPI_FLASH_QUAD_CE1,                &MSPI_GD25LB512_Quad_CE1_MSPIConfig},
    {AM_HAL_MSPI_FLASH_QUAD_CE0_1_4_4,          &MSPI_GD25LB512_Quad_CE0_MSPIConfig},
    {AM_HAL_MSPI_FLASH_QUAD_CE1_1_4_4,          &MSPI_GD25LB512_Quad_CE1_MSPIConfig},
};

#if defined(AM_PART_APOLLO4) || defined(AM_PART_APOLLO4B)
//
// Timing default setting
//
static am_devices_mspi_gd25lb512_timing_config_t TimingConfigDefault =
{
    .bTxNeg            = false,
    .bRxNeg            = false,
    .bRxCap            = false,
    .ui8TxDQSDelay     = 0,
    .ui8RxDQSDelay     = 15,
    .ui8Turnaround     = 6,
};
//
// Timing stored setting
//
static bool bSDRTimingConfigSaved = false;
static am_devices_mspi_gd25lb512_timing_config_t TimingConfigStored;


static const uint32_t ui32MspiXipBaseAddress[3] =
{
    0x14000000, // mspi0
    0x18000000, // mspi1
    0x1C000000, // mspi2
};

#endif
//
// Forward declarations.
//
static uint32_t am_devices_mspi_gd25lb512_command_write(void *pHandle,
                                                        uint8_t ui8Instr,
                                                        bool bSendAddr,
                                                        uint32_t ui32Addr,
                                                        uint32_t *pData,
                                                        uint32_t ui32NumBytes);
static uint32_t am_devices_mspi_gd25lb512_command_read(void *pHandle,
                                                       uint8_t ui8Instr,
                                                       bool bSendAddr,
                                                       uint32_t ui32Addr,
                                                       bool bTurnAround,
                                                       uint32_t *pData,
                                                       uint32_t ui32NumBytes);

//*****************************************************************************
//
// GigaDevice GD25LB Support
//
//*****************************************************************************

#define GD25LB512_REG_DEFAULT_DUMMY_CYCLE          0x0

#define GD25LB512_REG_DEFAULT_OTP                  0x0

#define GD25LB512_REG_DRIVE_STRENGTH_50OHM         0xFF
#define GD25LB512_REG_DRIVE_STRENGTH_35OHM         0xFE
#define GD25LB512_REG_DRIVE_STRENGTH_25OHM         0xFD
#define GD25LB512_REG_DRIVE_STRENGTH_18OHM         0xFC

#define GD25LB512_REG_DEFAULT_ODT_DLP_PROTECT      0x3C

#define GD25LB512_REG_ODT_DISABLE                  0x3
#define GD25LB512_REG_ODT_150OHM                   0x2
#define GD25LB512_REG_ODT_100OHM                   0x1
#define GD25LB512_REG_ODT_50OHM                    0x0

#define GD25LB512_REG_DLP_DISABLE                  0x1
#define GD25LB512_REG_DLP_ENABLE                   0x0

#define GD25LB512_REG_PROTECT_BP                   0x1
#define GD25LB512_REG_PROTECT_WPS                  0x0

#define GD25LB512_REG_ADDR_3_BYTE                  0xFF
#define GD25LB512_REG_ADDR_4_BYTE                  0xFE

#define GD25LB512_REG_XIP_DISABLE                  0xFF
#define GD25LB512_REG_XIP_ENABLE                   0xFE

#define GD25LB512_REG_WRAP_DISABLE                 0xFF
#define GD25LB512_REG_WRAP_64B                     0xFE
#define GD25LB512_REG_WRAP_32B                     0xFD
#define GD25LB512_REG_WRAP_16B                     0xFC

//
// Initialize the default value for the configuration register
//
static void am_devices_mspi_gd25lb512_get_default_regcfg(am_devices_mspi_gd25lb512_regcfg_t *pRegCfg)
{
    pRegCfg->ui8DummyCycle     = GD25LB512_REG_DEFAULT_DUMMY_CYCLE;     //default, according to specific command
    pRegCfg->ui8OTPConfig      = GD25LB512_REG_DEFAULT_OTP;             //default, security register unlocked
    pRegCfg->ui8DriveStrength  = GD25LB512_REG_DRIVE_STRENGTH_50OHM;    //default, 50 Ohm
    pRegCfg->ui8OdtDlpProtect  = GD25LB512_REG_DEFAULT_ODT_DLP_PROTECT; //default, ODT disabled, DLP disabled, BP protection
    pRegCfg->ui8AddrMode       = GD25LB512_REG_ADDR_3_BYTE;             //default, 3-byte address mode
    pRegCfg->ui8XipConfig      = GD25LB512_REG_XIP_DISABLE;             //default, XIP disabled
    pRegCfg->ui8WrapConfig     = GD25LB512_REG_WRAP_DISABLE;            //default, wrap disabled
}

//
// Set the dummy cycle according to frequency and mode
//
static uint32_t am_devices_mspi_gd25lb512_set_dummy_cycle(am_devices_mspi_gd25lb512_t *pFlash,
                                                          am_hal_mspi_clock_e eClockFreq,
                                                          bool bDDR, uint8_t *ui8DC)
{
    if (bDDR)
    {
        switch(eClockFreq)
        {
#if defined(AM_PART_APOLLO5_API)
            case AM_HAL_MSPI_CLK_250MHZ:
            case AM_HAL_MSPI_CLK_192MHZ:
                *ui8DC = 0;
                break;
            case AM_HAL_MSPI_CLK_125MHZ:
                *ui8DC = 6;
                break;
#endif
#if defined(AM_PART_APOLLO5_API) || defined(AM_PART_APOLLO4_API)
            case AM_HAL_MSPI_CLK_96MHZ:
                *ui8DC = 6;
                break;
#endif
            default:
                *ui8DC = 6;
                break;
        }
    }
    else
    {
        switch(eClockFreq)
        {
#if defined(AM_PART_APOLLO5_API)
            case AM_HAL_MSPI_CLK_250MHZ:
            case AM_HAL_MSPI_CLK_192MHZ:
                *ui8DC = 0;
                break;
            case AM_HAL_MSPI_CLK_125MHZ:
                *ui8DC = 8;
                break;
#endif
#if defined(AM_PART_APOLLO5_API) || defined(AM_PART_APOLLO4_API)
            case AM_HAL_MSPI_CLK_96MHZ:
                *ui8DC = 8;
                break;
#endif
            default:
                *ui8DC = 6;
                break;
        }
    }

#if defined(APOLLO5_FPGA)
    *ui8DC = 6;
#endif

    if (*ui8DC < 3 || *ui8DC > 30)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (pFlash->regCfg.ui8DummyCycle == GD25LB512_REG_DEFAULT_DUMMY_CYCLE &&
        *ui8DC == 6)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    }

    if (pFlash->regCfg.ui8DummyCycle != *ui8DC)
    {
        if (AM_HAL_STATUS_SUCCESS != am_devices_mspi_gd25lb512_command_write(pFlash->pMspiHandle,
                                                                             AM_DEVICES_MSPI_GD25LB512_WRITE_ENABLE,
                                                                             false, 0, NULL, 0))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        uint32_t ui32PIOBuffer = *ui8DC;
        if (AM_HAL_STATUS_SUCCESS != am_devices_mspi_gd25lb512_command_write(pFlash->pMspiHandle,
                                                                             AM_DEVICES_MSPI_GD25LB512_WRITE_VOL_CFG,
                                                                             true, 0x1, &ui32PIOBuffer, 1))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        pFlash->regCfg.ui8DummyCycle = *ui8DC;
    }
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//
// Device specific initialization function. Current state of the device is SPI Mode
//
static uint32_t
am_device_init_flash(void *pHandle)
{
    uint32_t      ui32Status;
    uint32_t      ui32PIOBuffer[4] = {0};
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Configure the MSPI_FLASH byte addressing mode.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_ENABLE, false, 0, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    switch (pFlash->currDevCfg.eAddrCfg)
    {
        case AM_HAL_MSPI_ADDR_1_BYTE:
        case AM_HAL_MSPI_ADDR_2_BYTE:
        case AM_HAL_MSPI_ADDR_3_BYTE:
            // Exit 4-byte mode.
            ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_EXIT_4B, false, 0, ui32PIOBuffer, 0);
            if (AM_HAL_STATUS_SUCCESS != ui32Status)
            {
                return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
            }
            break;
        case AM_HAL_MSPI_ADDR_4_BYTE:
            // Enter 4-byte mode.
            ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_ENTER_4B, false, 0, ui32PIOBuffer, 0);
            if (AM_HAL_STATUS_SUCCESS != ui32Status)
            {
                return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
            }
            break;
    }

    ui32Status = am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_VOL_CFG, true, 0x1, true, ui32PIOBuffer, 1);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    am_util_debug_printf("Volatile config reg dummy Cycle %d.\n", (uint8_t)ui32PIOBuffer[0]);

    //
    // Configure the MSPI_FLASH mode based on the MSPI configuration.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_ENABLE, false, 0, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    switch (pFlash->currDevCfg.eDeviceConfig)
    {
        case AM_HAL_MSPI_FLASH_SERIAL_CE0:
        case AM_HAL_MSPI_FLASH_SERIAL_CE1:
        case AM_HAL_MSPI_FLASH_QUAD_CE0_1_4_4:
        case AM_HAL_MSPI_FLASH_QUAD_CE1_1_4_4:
            break;
        case AM_HAL_MSPI_FLASH_QUAD_CE0:
        case AM_HAL_MSPI_FLASH_QUAD_CE1:
            ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_GD25LB512_ENTER_QPI_MODE, false, 0, ui32PIOBuffer, 0);
            if (AM_HAL_STATUS_SUCCESS != ui32Status)
            {
                return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
            }
            break;
        default:
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
            //break;
    }
    // Return status.
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//
// Device specific de-initialization function.
//
static uint32_t
am_device_deinit_flash(void *pHandle)
{
    uint32_t      ui32Status;

    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_GD25LB512_RETURN_TO_SPI_MODE, false, 0, NULL, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Generic Command Write function.
//
//*****************************************************************************
static uint32_t
am_devices_mspi_gd25lb512_command_write(void *pHandle, uint8_t ui8Instr, bool bSendAddr,
                                        uint32_t ui32Addr, uint32_t *pData,
                                        uint32_t ui32NumBytes)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;
    am_hal_mspi_pio_transfer_t  stMSPIFlashPIOTransaction = {0};

    //
    // Create the individual write transaction.
    //
    stMSPIFlashPIOTransaction.ui32NumBytes       = ui32NumBytes;
    stMSPIFlashPIOTransaction.eDirection         = AM_HAL_MSPI_TX;
    stMSPIFlashPIOTransaction.bSendAddr          = bSendAddr;
    stMSPIFlashPIOTransaction.ui32DeviceAddr     = ui32Addr;
    stMSPIFlashPIOTransaction.bSendInstr         = true;
    stMSPIFlashPIOTransaction.ui16DeviceInstr    = ui8Instr;
    stMSPIFlashPIOTransaction.bTurnaround        = false;
#if 0 // A3DS-25 Deprecate MSPI CONT
    stMSPIFlashPIOTransaction.bContinue          = false;
#endif // A3DS-25
    stMSPIFlashPIOTransaction.pui32Buffer        = pData;
#if defined(AM_PART_APOLLO4_API)
#if defined(AM_PART_APOLLO4)
    stMSPIFlashPIOTransaction.eDeviceNum         = AM_HAL_MSPI_DEVICE0;
#endif
    stMSPIFlashPIOTransaction.bDCX               = false;
    stMSPIFlashPIOTransaction.bEnWRLatency       = false;
    stMSPIFlashPIOTransaction.bContinue          = false;
#endif

    //
    // Execute the transction over MSPI.
    //
    ui32Status = am_hal_mspi_blocking_transfer(pFlash->pMspiHandle, &stMSPIFlashPIOTransaction,
                 AM_DEVICES_MSPI_GD25LB512_TIMEOUT);
    return ui32Status;
}

//*****************************************************************************
//
// Generic Command Read function.
//
//*****************************************************************************
static uint32_t
am_devices_mspi_gd25lb512_command_read(void *pHandle, uint8_t ui8Instr, bool bSendAddr,
                                       uint32_t ui32Addr, bool bTurnAround, uint32_t *pData,
                                       uint32_t ui32NumBytes)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;
    am_hal_mspi_pio_transfer_t      stMSPIFlashPIOTransaction = {0};

#if defined(AM_PART_APOLLO4_API)
    // Check if we are trying to send the command at 96MHz.
    if (AM_HAL_MSPI_CLK_96MHZ == pFlash->currDevCfg.eClockFreq)
    {
      am_hal_mspi_clock_e clkCfg;
      clkCfg = AM_HAL_MSPI_CLK_48MHZ;  // Set the clock to 48MHz for commmands.
      ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_CLOCK_CONFIG, &clkCfg);
      if (AM_HAL_STATUS_SUCCESS != ui32Status)
      {
        return ui32Status;
      }
    }
#endif

    //
    // Create the individual write transaction.
    //
    stMSPIFlashPIOTransaction.eDirection         = AM_HAL_MSPI_RX;
    stMSPIFlashPIOTransaction.bSendAddr          = bSendAddr;
    stMSPIFlashPIOTransaction.ui32DeviceAddr     = ui32Addr;
    stMSPIFlashPIOTransaction.bSendInstr         = true;
    stMSPIFlashPIOTransaction.ui16DeviceInstr    = ui8Instr;
    stMSPIFlashPIOTransaction.bTurnaround        = bTurnAround;
#if 0 // A3DS-25 Deprecate MSPI CONT
    stMSPIFlashPIOTransaction.bContinue          = false;
#endif // A3DS-25
    stMSPIFlashPIOTransaction.ui32NumBytes     = ui32NumBytes;
    stMSPIFlashPIOTransaction.pui32Buffer        = pData;
#if defined(AM_PART_APOLLO4_API)
#if defined(AM_PART_APOLLO4)
    stMSPIFlashPIOTransaction.eDeviceNum         = AM_HAL_MSPI_DEVICE0;
#endif
    stMSPIFlashPIOTransaction.bDCX               = false;
    stMSPIFlashPIOTransaction.bEnWRLatency       = false;
    stMSPIFlashPIOTransaction.bContinue          = false;
#endif
    //
    // Execute the transction over MSPI.
    //
    ui32Status = am_hal_mspi_blocking_transfer(pFlash->pMspiHandle, &stMSPIFlashPIOTransaction,
                 AM_DEVICES_MSPI_GD25LB512_TIMEOUT);
#if defined(AM_PART_APOLLO4_API)
    // Check if we had to step down the command to 48MHz.
    if (AM_HAL_MSPI_CLK_96MHZ == pFlash->currDevCfg.eClockFreq)
    {
      am_hal_mspi_clock_e clkCfg;
      clkCfg = AM_HAL_MSPI_CLK_96MHZ;  // Reset the clock to 96MHz.
      ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_CLOCK_CONFIG, &clkCfg);
    }
#endif
    return ui32Status;
}

static uint32_t
am_devices_mspi_device_reconfigure(void * pHandle, am_hal_mspi_dev_config_t *pConfig)
{
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    // Disable MSPI defore re-configuring it
    uint32_t ui32Status = am_hal_mspi_disable(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Error - Failed to disble mspi.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Re-Configure the MSPI for the requested operation mode.
    //
    ui32Status = am_hal_mspi_device_configure(pFlash->pMspiHandle, pConfig);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Error - Failed to configure mspi.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    // Re-Enable MSPI
    ui32Status = am_hal_mspi_enable(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        am_util_stdio_printf("Error - Failed to configure mspi.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //Restore GPIO configuration
    am_bsp_mspi_pins_enable(pFlash->ui32Module, pConfig->eDeviceConfig);

    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

static uint32_t am_devices_mspi_gd25lb512_enter_command_mode(void *pHandle)
{
    //
    // For 1-4-4 and 1-1-4 modes to return to 1-1-1 for sending commands
    //
    uint32_t ui32Status = AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    if (pFlash->currDevCfg.eDeviceConfig != pFlash->cmdDevCfg.eDeviceConfig)
    {
        if (pFlash->currDevCfg.eDeviceConfig != AM_HAL_MSPI_FLASH_QUAD_CE0 &&
            pFlash->currDevCfg.eDeviceConfig != AM_HAL_MSPI_FLASH_QUAD_CE1)
        {
            ui32Status = am_devices_mspi_device_reconfigure(pHandle, &pFlash->cmdDevCfg);
        }
    }
    return ui32Status;
}

static uint32_t am_devices_mspi_gd25lb512_exit_command_mode(void *pHandle)
{
    //
    // For 1-4-4 and 1-1-4 modes to return to original modes after sending commands
    //
    uint32_t ui32Status = AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    if (pFlash->currDevCfg.eDeviceConfig != pFlash->cmdDevCfg.eDeviceConfig)
    {
        if (pFlash->currDevCfg.eDeviceConfig != AM_HAL_MSPI_FLASH_QUAD_CE0 &&
            pFlash->currDevCfg.eDeviceConfig != AM_HAL_MSPI_FLASH_QUAD_CE1)
        {
            ui32Status = am_devices_mspi_device_reconfigure(pHandle, &pFlash->currDevCfg);
        }
    }
    return ui32Status;
}

static uint32_t am_devices_mspi_gd25lb512_return_to_spi_mode(void *pHandle)
{
    uint32_t ui32Status = AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;
    am_hal_mspi_dev_config_t *pDevConfigTemp;


    switch(pFlash->currDevCfg.eDeviceConfig)
    {
        case AM_HAL_MSPI_FLASH_SERIAL_CE0:
        case AM_HAL_MSPI_FLASH_QUAD_CE0_1_4_4:
        case AM_HAL_MSPI_FLASH_QUAD_CE0:
            pDevConfigTemp = &MSPI_GD25LB512_Quad_CE0_MSPIConfig;
            break;

        case AM_HAL_MSPI_FLASH_SERIAL_CE1:
        case AM_HAL_MSPI_FLASH_QUAD_CE1_1_4_4:
        case AM_HAL_MSPI_FLASH_QUAD_CE1:
            pDevConfigTemp = &MSPI_GD25LB512_Quad_CE1_MSPIConfig;
            break;
        default:
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    ui32Status = am_devices_mspi_device_reconfigure(pHandle, pDevConfigTemp);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // For 4-4-4 only
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_GD25LB512_RETURN_TO_SPI_MODE, false, 0, NULL, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    ui32Status = am_devices_mspi_device_reconfigure(pHandle, &pFlash->currDevCfg);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    return ui32Status;
}

//
// Callback function.
//
static void
pfnMSPI_GD25LB512_Callback(void *pCallbackCtxt, uint32_t status)
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
//! @brief  MSPI initialization.
//! @param  ui32Module
//! @param  psMSPISettings
//! @param  pFlash
//! @return ui32Status
//
//*****************************************************************************
static uint32_t am_devices_mspi_peripheral_init(uint32_t ui32Module,
                                                am_devices_mspi_gd25lb512_config_t *psMSPISettings,
                                                am_devices_mspi_gd25lb512_t *pFlash)
{
    bool bFound = false;
    for (uint32_t i = 0; i < (sizeof(g_GD25LB512_DevConfig) / sizeof(g_GD25LB512_DevConfig[0])); i++)
    {
        if (psMSPISettings->eDeviceConfig == g_GD25LB512_DevConfig[i].eHalDeviceEnum)
        {
            bFound = true;
            break;
        }
    }

    if ( !bFound )
    {
        am_util_debug_printf("Error - Incorrect eDeviceConfig.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Configure the MSPI mode based on device default
    //
    switch (psMSPISettings->eDeviceConfig)
    {
        case AM_HAL_MSPI_FLASH_SERIAL_CE0:
        case AM_HAL_MSPI_FLASH_QUAD_CE0:
        case AM_HAL_MSPI_FLASH_QUAD_CE0_1_4_4:
            pFlash->cmdDevCfg = MSPI_GD25LB512_Serial_CE0_MSPIConfig;
            break;
        case AM_HAL_MSPI_FLASH_SERIAL_CE1:
        case AM_HAL_MSPI_FLASH_QUAD_CE1:
        case AM_HAL_MSPI_FLASH_QUAD_CE1_1_4_4:
            pFlash->cmdDevCfg = MSPI_GD25LB512_Serial_CE1_MSPIConfig;
            break;
        default:
            am_util_stdio_printf("Error - Device Config is unavailable.\n");
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

#if !defined(AM_PART_APOLLO4_API) && !defined(AM_PART_APOLLO5_API)
    pFlash->cmdDevCfg.pTCB = psMSPISettings->pNBTxnBuf;
    pFlash->cmdDevCfg.ui32TCBSize = psMSPISettings->ui32NBTxnBufLength;
    pFlash->cmdDevCfg.scramblingStartAddr = psMSPISettings->ui32ScramblingStartAddr;
    pFlash->cmdDevCfg.scramblingEndAddr = psMSPISettings->ui32ScramblingEndAddr;
#endif

    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_initialize(ui32Module, &pFlash->pMspiHandle))
    {
        am_util_stdio_printf("Error - Failed to initialize MSPI.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_power_control(pFlash->pMspiHandle, AM_HAL_SYSCTRL_WAKE, false))
    {
        am_util_stdio_printf("Error - Failed to power on MSPI.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    pFlash->ui32Module = ui32Module;
#if defined(AM_PART_APOLLO4_API) || defined(AM_PART_APOLLO5_API)
    pFlash->pMspiCfg->ui32TCBSize = psMSPISettings->ui32NBTxnBufLength;
    pFlash->pMspiCfg->pTCB = psMSPISettings->pNBTxnBuf;
    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_configure(pFlash->pMspiHandle, pFlash->pMspiCfg))
    {
        am_util_stdio_printf("Error - Failed to configure MSPI device.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
#endif

    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_device_configure(pFlash->pMspiHandle, &pFlash->cmdDevCfg))
    {
        am_util_stdio_printf("Error - Failed to configure MSPI.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_enable(pFlash->pMspiHandle))
    {
        am_util_stdio_printf("Error - Failed to enable MSPI.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    am_bsp_mspi_pins_enable(ui32Module, pFlash->cmdDevCfg.eDeviceConfig);

    pFlash->currDevCfg = pFlash->cmdDevCfg;

    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Initialize the mspi flash driver.
//!
//! @param ui32Module - module instance.
//! @param psMSPISettings - MSPI device structure describing the target spiflash.
//! @param ppHandle - returns the handle for the MSPI Device instance.
//! @param ppMspiHandle - returns the handle for the MSPI instance.
//!
//! This function should be called before any other am_devices_mspi_gd25lb512
//! functions. It is used to set tell the other functions how to communicate
//! with the external flash hardware.
//!
//! @return 32-bit status.
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_init(uint32_t ui32Module, am_devices_mspi_gd25lb512_config_t *psMSPISettings, void **ppHandle, void **ppMspiHandle)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = &gAmGd25lb[psMSPISettings->ui32DeviceNum];

    if ((ui32Module > AM_REG_MSPI_NUM_MODULES) || (psMSPISettings == NULL))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if ( psMSPISettings->ui32DeviceNum > AM_DEVICES_MSPI_GD25LB512_MAX_DEVICE_NUM )
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (pFlash->bOccupied == false)
    {
        am_devices_mspi_gd25lb512_get_default_regcfg(&pFlash->regCfg);
        pFlash->pMspiCfg      = &gGDMspiCfg[ui32Module];
        pFlash->pXipCfg       = &gGDXipConfig[ui32Module];
        pFlash->pRxCfg        = &gGDMspiRxCfg;
        pFlash->pTimingCfg    = &gGDTimeCfg[ui32Module];
        memset(&pFlash->currDevCfg, 0, sizeof(am_hal_mspi_dev_config_t));
        memset(&pFlash->cmdDevCfg, 0, sizeof(am_hal_mspi_dev_config_t));
    }
    else
    {
        am_util_debug_printf("Device %d occupied!!\n", psMSPISettings->ui32DeviceNum);
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Enable fault detection.
    //
#if !defined(AM_PART_APOLLO5_API)
#if defined(AM_PART_APOLLO4_API)
    am_hal_fault_capture_enable();
#elif AM_PART_APOLLO3_API
    am_hal_mcuctrl_control(AM_HAL_MCUCTRL_CONTROL_FAULT_CAPTURE_ENABLE, 0);
#else
    am_hal_mcuctrl_fault_capture_enable();
#endif
#endif

    if (AM_HAL_STATUS_SUCCESS != am_devices_mspi_peripheral_init(ui32Module, psMSPISettings, pFlash))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Send 0xFF in quad mode regardless as we initializing in serial mode.
    //
    if (AM_HAL_STATUS_SUCCESS != am_devices_mspi_gd25lb512_return_to_spi_mode((void*)pFlash))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_devices_mspi_gd25lb512_reset((void*)pFlash))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Switch to big endian for accessing flash registers
    //
    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_BIG_ENDIAN, NULL);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
      return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Read flash ID
    //
    if (AM_HAL_STATUS_SUCCESS != am_devices_mspi_gd25lb512_id((void *)pFlash))
    {
       return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Switch back to little endian for accessing flash memory
    //
    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_LITTLE_ENDIAN, NULL);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
      return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    for (uint32_t i = 0; i < (sizeof(g_GD25LB512_DevConfig) / sizeof(g_GD25LB512_DevConfig[0])); i++)
    {
        if (psMSPISettings->eDeviceConfig == g_GD25LB512_DevConfig[i].eHalDeviceEnum)
        {
            pFlash->currDevCfg                        = *g_GD25LB512_DevConfig[i].psDevConfig;
            pFlash->currDevCfg.eClockFreq             = psMSPISettings->eClockFreq;
            pFlash->currDevCfg.eDeviceConfig          = psMSPISettings->eDeviceConfig;
#if !defined(AM_PART_APOLLO4_API) && !defined(AM_PART_APOLLO5_API)
            pFlash->currDevCfg.pTCB                   = psMSPISettings->pNBTxnBuf;
            pFlash->currDevCfg.ui32TCBSize            = psMSPISettings->ui32NBTxnBufLength;
            pFlash->currDevCfg.scramblingStartAddr    = psMSPISettings->ui32ScramblingStartAddr;
            pFlash->currDevCfg.scramblingEndAddr      = psMSPISettings->ui32ScramblingEndAddr;
#endif
            break;
        }
    }

    //
    // Device specific MSPI Flash initialization.
    //
    ui32Status = am_device_init_flash((void *)pFlash);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (psMSPISettings->eDeviceConfig != AM_HAL_MSPI_FLASH_SERIAL_CE0 &&
        psMSPISettings->eDeviceConfig != AM_HAL_MSPI_FLASH_SERIAL_CE1)
    {
        ui32Status = am_devices_mspi_gd25lb512_set_dummy_cycle(pFlash, pFlash->currDevCfg.eClockFreq,
                                                            pFlash->currDevCfg.bEmulateDDR,
                                                            &pFlash->currDevCfg.ui8TurnAround);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }

    if (pFlash->currDevCfg.bEmulateDDR)
    {
        pFlash->currDevCfg.ui8TurnAround *= 2;
    }

    //
    // Disable MSPI defore re-configuring it.
    //
    ui32Status = am_hal_mspi_disable(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Re-Configure the MSPI for the requested operation mode.
    //
    ui32Status = am_hal_mspi_device_configure(pFlash->pMspiHandle, &pFlash->currDevCfg);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

#if defined(AM_PART_APOLLO4_API) || defined(AM_PART_APOLLO5_API)
    pFlash->pXipCfg->scramblingStartAddr = psMSPISettings->ui32ScramblingStartAddr;
    pFlash->pXipCfg->scramblingEndAddr = psMSPISettings->ui32ScramblingEndAddr;
    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_XIP_CONFIG, pFlash->pXipCfg);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
#endif

    //
    // Re-Enable MSPI
    //
    ui32Status = am_hal_mspi_enable(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Configure the MSPI pins.
    //
    am_bsp_mspi_pins_enable(ui32Module, pFlash->currDevCfg.eDeviceConfig);

    //
    // Enable MSPI interrupts.
    //
    ui32Status = am_hal_mspi_interrupt_clear(pFlash->pMspiHandle, AM_HAL_MSPI_INT_CQUPD | AM_HAL_MSPI_INT_ERR);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    ui32Status = am_hal_mspi_interrupt_enable(pFlash->pMspiHandle, AM_HAL_MSPI_INT_CQUPD | AM_HAL_MSPI_INT_ERR);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Return the handle.
    //
    pFlash->bOccupied = true;
    *ppMspiHandle = pFlash->pMspiHandle;
    *ppHandle = (void *)pFlash;

    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief De-Initialization the mspi flash driver.
//!
//! @param pHandle - Flash device handle.
//!
//! This function reverses the initialization.
//!
//! @return 32-bit status.
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_deinit(void *pHandle)
{
    uint32_t      ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Device specific MSPI Flash de-initialization.
    //
    ui32Status = am_device_deinit_flash(pHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Disable and clear the interrupts to start with.
    //
    ui32Status = am_hal_mspi_interrupt_disable(pFlash->pMspiHandle, 0xFFFFFFFF);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    ui32Status = am_hal_mspi_interrupt_clear(pFlash->pMspiHandle, 0xFFFFFFFF);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Disable the MSPI instance.
    //
    ui32Status = am_hal_mspi_disable(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    if (AM_HAL_STATUS_SUCCESS != am_hal_mspi_power_control(pFlash->pMspiHandle, AM_HAL_SYSCTRL_DEEPSLEEP, false))
    {
        am_util_debug_printf("Error - Failed to power on MSPI.\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Deinitialize the MSPI instance.
    //
    ui32Status = am_hal_mspi_deinitialize(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Free this device handle
    //
    pFlash->bOccupied = false;
    //
    // Clear the Flash Caching.
    //
#if !defined(AM_PART_APOLLO4_API) && !defined(AM_PART_APOLLO5_API)
#if AM_CMSIS_REGS
    CACHECTRL->CACHECFG = 0;
#else // AM_CMSIS_REGS
    AM_REG(CACHECTRL, CACHECFG) = 0;
#endif // AM_CMSIS_REGS
#endif // !AM_PART_APOLLO4
    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Reset the external flash.
//!
//! @param pHandle - Flash device handle.
//!
//! This function reset the external flash, and returns the result as an 32-bit
//! unsigned integer value.
//!
//! @return 32-bit status.
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_reset(void *pHandle)
{
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;
    uint32_t ui32Status;
    uint32_t ui32PinNum = 0xFFFFFFFF;
    am_hal_gpio_pincfg_t sPinCfg;

    am_bsp_mspi_reset_pincfg_get(pFlash->ui32Module, pFlash->currDevCfg.eDeviceConfig, &ui32PinNum, &sPinCfg);
    if (ui32PinNum != 0xFFFFFFFF)
    {
        am_hal_gpio_pinconfig(ui32PinNum, sPinCfg);
        am_hal_gpio_output_clear(ui32PinNum);
        am_util_delay_cycles(10);   // minimal tRLRH is 1us
        am_hal_gpio_output_set(ui32PinNum);
        am_util_delay_ms(25);       // delay minimal tRB just to be safe
    }

    if (am_devices_mspi_gd25lb512_enter_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_RESET_ENABLE, false, 0, NULL, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_RESET_MEMORY, false, 0, NULL, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    uint32_t ui32Flag = 0;
    while((ui32Flag & 0x80808080) == 0)
    {
        ui32Status = am_devices_mspi_gd25lb512_command_read(pHandle, 0x70, false, 0, false, &ui32Flag, 4);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }

    if (am_devices_mspi_gd25lb512_exit_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Reads the ID of the external flash .
//!
//! @param pHandle - Flash device handle.
//!
//! This function reads the device ID register of the external flash, and returns
//! the result as an 32-bit unsigned integer value.
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_id(void *pHandle)
{
    uint32_t    ui32Status;
    uint32_t    ui32DeviceID;

    if (am_devices_mspi_gd25lb512_enter_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Send the command sequence to read the Device ID and return status.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_ID, false, 0, false, &ui32DeviceID, 4);

    if (am_devices_mspi_gd25lb512_exit_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (((ui32DeviceID & AM_DEVICES_MSPI_GD25LB512_ID_MASK) == AM_DEVICES_MSPI_GD25LB512_ID) &&
            (AM_HAL_STATUS_SUCCESS == ui32Status))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    }
    else
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
}

//*****************************************************************************
//
//! @brief Reads the current status of the external flash
//!
//! @param pHandle - Flash device handle.
//! @param pStatus - Flash device status.
//!
//! This function reads the status register of the external flash, and returns
//! the result as an 8-bit unsigned integer value. The processor will block
//! during the data transfer process, but will return as soon as the status
//! register had been read.
//!
//! @return 32-bit status..
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_status(void *pHandle, uint32_t *pStatus)
{
    uint32_t      ui32Status;

    if (am_devices_mspi_gd25lb512_enter_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Send the command sequence to read the device status.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_STATUS, false, 0, false, pStatus, 1);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (am_devices_mspi_gd25lb512_exit_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}
//*****************************************************************************
//
//! @brief Reads the contents of the external flash into a buffer.
//!
//! @param pHandle - Flash device handle.
//! @param pui8RxBuffer - Buffer to store the received data from the flash
//! @param ui32ReadAddress - Address of desired data in external flash
//! @param ui32NumBytes - Number of bytes to read from external flash
//! @param ui32PauseCondition - Pause condition before transaction is executed
//! @param ui32StatusSetClr - Post-transaction CQ condition
//! @param pfnCallback - Post-transaction callback function
//! @param pCallbackCtxt - Post-transaction callback context
//!
//! This function reads the external flash at the provided address and stores
//! the received data into the provided buffer location. This function will
//! only store ui32NumBytes worth of data.  The Command Queue pre and post
//! transaction conditions and a callback function and context are also
//! provided.
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_read_adv(void *pHandle, uint8_t *pui8RxBuffer,
                                   uint32_t ui32ReadAddress,
                                   uint32_t ui32NumBytes,
                                   uint32_t ui32PauseCondition,
                                   uint32_t ui32StatusSetClr,
                                   am_hal_mspi_callback_t pfnCallback,
                                   void *pCallbackCtxt)
{
    am_hal_mspi_dma_transfer_t    Transaction;
    uint32_t                      ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Set the DMA priority
    //
    Transaction.ui8Priority = 1;

    //
    // Set the transfer direction to RX (Read)
    //
    Transaction.eDirection = AM_HAL_MSPI_RX;

    //
    // Set the transfer count in bytes.
    //
    Transaction.ui32TransferCount = ui32NumBytes;

    //
    // Set the address to read data from.
    //
    Transaction.ui32DeviceAddress = ui32ReadAddress;

    //
    // Set the target SRAM buffer address.
    //
    Transaction.ui32SRAMAddress = (uint32_t)pui8RxBuffer;

    //
    // Clear the CQ stimulus.
    //
    Transaction.ui32PauseCondition = ui32PauseCondition;

    //
    // Clear the post-processing
    //
    Transaction.ui32StatusSetClr = ui32StatusSetClr;
#if defined(AM_PART_APOLLO4)
    Transaction.eDeviceNum         = AM_HAL_MSPI_DEVICE0;
#endif

    //
    // Check the transaction status.
    //
    ui32Status = am_hal_mspi_nonblocking_transfer(pFlash->pMspiHandle, &Transaction,
                 AM_HAL_MSPI_TRANS_DMA, pfnCallback, pCallbackCtxt);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Reads the contents of the external flash into a buffer.
//!
//! @param pui8RxBuffer - Buffer to store the received data from the flash
//! @param ui32ReadAddress - Address of desired data in external flash
//! @param ui32NumBytes - Number of bytes to read from external flash
//! @param bWaitForCompletion - Wait for transaction completion before exiting
//!
//! This function reads the external flash at the provided address and stores
//! the received data into the provided buffer location. This function will
//! only store ui32NumBytes worth of data.If the bWaitForCompletion is true,
//! then the function will poll for DMA completion indication flag before
//! returning.
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_read(void *pHandle, uint8_t *pui8RxBuffer,
                               uint32_t ui32ReadAddress,
                               uint32_t ui32NumBytes,
                               bool bWaitForCompletion)
{
    am_hal_mspi_dma_transfer_t    Transaction;
    uint32_t                      ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Set the DMA priority
    //
    Transaction.ui8Priority = 1;

    //
    // Set the transfer direction to RX (Read)
    //
    Transaction.eDirection = AM_HAL_MSPI_RX;

    //
    // Set the transfer count in bytes.
    //
    Transaction.ui32TransferCount = ui32NumBytes;

    //
    // Set the address to read data from.
    //
    Transaction.ui32DeviceAddress = ui32ReadAddress;

    //
    // Set the target SRAM buffer address.
    //

    Transaction.ui32SRAMAddress = (uint32_t)pui8RxBuffer;

    //
    // Clear the CQ stimulus.
    //
    Transaction.ui32PauseCondition = 0;

    //
    // Clear the post-processing
    //
    Transaction.ui32StatusSetClr = 0;
#if defined(AM_PART_APOLLO4)
    Transaction.eDeviceNum         = AM_HAL_MSPI_DEVICE0;
#endif
    if (bWaitForCompletion)
    {
        //
        // Start the transaction.
        //
        volatile uint32_t ui32DMAStatus = 0xFFFFFFFF;
        ui32Status = am_hal_mspi_nonblocking_transfer(pFlash->pMspiHandle, &Transaction,
                                                      AM_HAL_MSPI_TRANS_DMA, pfnMSPI_GD25LB512_Callback, (void *)&ui32DMAStatus);

        //
        // Check the transaction status.
        //
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }

        //
        // Wait for DMA Complete or Timeout
        //
        for (uint32_t i = 0; i < AM_DEVICES_MSPI_GD25LB512_TIMEOUT; i++)
        {
#if defined(AM_PART_APOLLO4) || defined(AM_PART_APOLLO4B)
            if ( (AM_HAL_STATUS_SUCCESS == ui32DMAStatus) || (AM_HAL_MSPI_FIFO_FULL_CONDITION == ui32DMAStatus) )
            {
                break;
            }
#else
            if (AM_HAL_STATUS_SUCCESS == ui32DMAStatus)
            {
                break;
            }
#endif

            //
            // Call the BOOTROM cycle function to delay for about 1 microsecond.
            //
            am_util_delay_us(1);
        }

#if defined(AM_PART_APOLLO4) || defined(AM_PART_APOLLO4B)
        if (AM_HAL_MSPI_FIFO_FULL_CONDITION == ui32DMAStatus)
        {
            am_hal_gpio_output_toggle(22);
            return AM_HAL_MSPI_FIFO_FULL_CONDITION;
        }
        else if (AM_HAL_STATUS_SUCCESS == ui32DMAStatus)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
        }
#else
        if (AM_HAL_STATUS_SUCCESS == ui32DMAStatus)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
        }
#endif
        else
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }
    else
    {
        //
        // Check the transaction status.
        //
        ui32Status = am_hal_mspi_nonblocking_transfer(pFlash->pMspiHandle, &Transaction,
                     AM_HAL_MSPI_TRANS_DMA, NULL, NULL);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }

    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

static uint32_t
mspi_gd25lb512_dma_blocking_read(void *pHandle, uint8_t *pui8RxBuffer,
                                 uint32_t ui32ReadAddress,
                                 uint32_t ui32NumBytes)
{
    am_hal_mspi_dma_transfer_t    Transaction;
    uint32_t                      ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    // Set the DMA priority
    Transaction.ui8Priority = 1;

    // Set the transfer direction to RX (Read)
    Transaction.eDirection = AM_HAL_MSPI_RX;

    // Set the transfer count in bytes.
    Transaction.ui32TransferCount = ui32NumBytes;

    // Set the address to read data from.
    Transaction.ui32DeviceAddress = ui32ReadAddress;

    // Set the target SRAM buffer address.
    Transaction.ui32SRAMAddress = (uint32_t)pui8RxBuffer;

    // Clear the CQ stimulus.
    Transaction.ui32PauseCondition = 0;
    // Clear the post-processing
    Transaction.ui32StatusSetClr = 0;

#if defined(AM_PART_APOLLO4)
    Transaction.eDeviceNum         = AM_HAL_MSPI_DEVICE0;
#endif

    // Start the transaction.
    volatile uint32_t ui32DMAStatus = 0xFFFFFFFF;
        ui32Status = am_hal_mspi_nonblocking_transfer(pFlash->pMspiHandle, &Transaction,
                                                      AM_HAL_MSPI_TRANS_DMA, pfnMSPI_GD25LB512_Callback, (void *)&ui32DMAStatus);

    // Check the transaction status.
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    // Wait for DMA Complete or Timeout
    for (uint32_t i = 0; i < AM_DEVICES_MSPI_GD25LB512_TIMEOUT; i++)
    {

        // check DMA status without using ISR
        am_hal_mspi_interrupt_status_get(pFlash->pMspiHandle, &ui32Status, false);
        am_hal_mspi_interrupt_clear(pFlash->pMspiHandle, ui32Status);
        am_hal_mspi_interrupt_service(pFlash->pMspiHandle, ui32Status);

#if defined(AM_PART_APOLLO4) || defined(AM_PART_APOLLO4B)
        if ( (AM_HAL_STATUS_SUCCESS == ui32DMAStatus) || (AM_HAL_MSPI_FIFO_FULL_CONDITION == ui32DMAStatus) )
        {
            break;
        }
#else
        if (AM_HAL_STATUS_SUCCESS == ui32DMAStatus)
        {
            break;
        }
#endif
        //
        // Call the BOOTROM cycle function to delay for about 1 microsecond.
        //
        am_util_delay_us(1);
    }

#if defined(AM_PART_APOLLO4) || defined(AM_PART_APOLLO4B)
    if (AM_HAL_MSPI_FIFO_FULL_CONDITION == ui32DMAStatus)
    {
        return AM_HAL_MSPI_FIFO_FULL_CONDITION;
    }
    else if (AM_HAL_STATUS_SUCCESS == ui32DMAStatus)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    }
#else
    if (AM_HAL_STATUS_SUCCESS == ui32DMAStatus)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    }
#endif
    else
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
}

//*****************************************************************************
//
//! @brief High priority Reads the contents of the external flash into a buffer.
//!
//! @param pHandle - Flash device handle.
//! @param pui8RxBuffer - Buffer to store the received data from the flash
//! @param ui32ReadAddress - Address of desired data in external flash
//! @param ui32NumBytes - Number of bytes to read from external flash
//! @param bWaitForCompletion - Wait for transaction completion before exiting
//!
//! This function reads the external flash at the provided address and stores
//! the received data into the provided buffer location. This function will
//! only store ui32NumBytes worth of data.
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_read_hiprio(void *pHandle, uint8_t *pui8RxBuffer,
                                      uint32_t ui32ReadAddress,
                                      uint32_t ui32NumBytes,
                                      bool bWaitForCompletion)
{
    am_hal_mspi_dma_transfer_t    Transaction;
    uint32_t                      ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Set the DMA priority
    //
    Transaction.ui8Priority = 1;

    //
    // Set the transfer direction to RX (Read)
    //
    Transaction.eDirection = AM_HAL_MSPI_RX;

    //
    // Set the transfer count in bytes.
    //
    Transaction.ui32TransferCount = ui32NumBytes;

    //
    // Set the address to read data from.
    //
    Transaction.ui32DeviceAddress = ui32ReadAddress;

    //
    // Set the target SRAM buffer address.
    //
    Transaction.ui32SRAMAddress = (uint32_t)pui8RxBuffer;

    //
    // Clear the CQ stimulus.
    //
    Transaction.ui32PauseCondition = 0;

    //
    // Clear the post-processing
    //
    Transaction.ui32StatusSetClr = 0;
#if defined(AM_PART_APOLLO4)
    Transaction.eDeviceNum         = AM_HAL_MSPI_DEVICE0;
#endif
    if (bWaitForCompletion)
    {
        //
        // Start the transaction.
        //
        volatile bool bDMAComplete = false;
        ui32Status = am_hal_mspi_highprio_transfer(pFlash->pMspiHandle, &Transaction, AM_HAL_MSPI_TRANS_DMA, pfnMSPI_GD25LB512_Callback, (void *)&bDMAComplete);

        //
        // Check the transaction status.
        //
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        //
        // Wait for DMA Complete or Timeout
        //
        for (uint32_t i = 0; i < AM_DEVICES_MSPI_GD25LB512_TIMEOUT; i++)
        {
            if (bDMAComplete)
            {
                break;
            }
            //
            // Call the BOOTROM cycle function to delay for about 1 microsecond.
            //
            am_util_delay_us(1);
        }
        //
        // Check the status.
        //
        if (!bDMAComplete)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }
    else
    {
        //
        // Check the transaction status.
        //
        ui32Status = am_hal_mspi_highprio_transfer(pFlash->pMspiHandle, &Transaction,
                     AM_HAL_MSPI_TRANS_DMA, NULL, NULL);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }
    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Programs the given range of flash addresses.
//!
//! @param ui32DeviceNumber - Device number of the external flash
//! @param pui8TxBuffer - Buffer to write the external flash data from
//! @param ui32WriteAddress - Address to write to in the external flash
//! @param ui32NumBytes - Number of bytes to write to the external flash
//!
//! This function uses the data in the provided pui8TxBuffer and copies it to
//! the external flash at the address given by ui32WriteAddress. It will copy
//! exactly ui32NumBytes of data from the original pui8TxBuffer pointer. The
//! user is responsible for ensuring that they do not overflow the target flash
//! memory or underflow the pui8TxBuffer array
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_write(void *pHandle, uint8_t *pui8TxBuffer,
                                uint32_t ui32WriteAddress,
                                uint32_t ui32NumBytes,
                                bool bWaitForCompletion)
{
    am_hal_mspi_dma_transfer_t    Transaction;
    bool                          bWriteComplete = false;
    uint32_t                      ui32BytesLeft = ui32NumBytes;
    uint32_t                      ui32PageAddress = ui32WriteAddress;
    uint32_t                      ui32BufferAddress = (uint32_t)pui8TxBuffer;
    uint32_t                      ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;
    uint32_t      ui32PIOBuffer[32] = {0};
    while (ui32BytesLeft > 0)
    {

        if (am_devices_mspi_gd25lb512_enter_command_mode(pHandle))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        //
        // Send the command sequence to enable writing.
        //
        ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_ENABLE, false, 0, ui32PIOBuffer, 0);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }

        if (am_devices_mspi_gd25lb512_exit_command_mode(pHandle))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }

        //
        // Set the DMA priority
        //
        Transaction.ui8Priority = 1;
        // Set the transfer direction to TX (Write)
        Transaction.eDirection = AM_HAL_MSPI_TX;
        if (ui32BytesLeft > AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE)
        {
            //
            // Set the transfer count in bytes.
            //
            Transaction.ui32TransferCount = AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE;
            ui32BytesLeft -= AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE;
        }
        else
        {
            //
            // Set the transfer count in bytes.
            //
            Transaction.ui32TransferCount = ui32BytesLeft;
            ui32BytesLeft = 0;
        }

        // Set the address to read data to.
        Transaction.ui32DeviceAddress = ui32PageAddress;
        ui32PageAddress += AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE;

        //
        // Set the source SRAM buffer address.
        //
        Transaction.ui32SRAMAddress = ui32BufferAddress;
        ui32BufferAddress += AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE;

        //
        // Clear the CQ stimulus.
        //
        Transaction.ui32PauseCondition = 0;

        //
        // Clear the post-processing
        //
        Transaction.ui32StatusSetClr = 0;
#if defined(AM_PART_APOLLO4)
        Transaction.eDeviceNum         = AM_HAL_MSPI_DEVICE0;
#endif
        //
        // Start the transaction.
        //
        volatile uint32_t ui32DMAStatus = 0xFFFFFFFF;
        ui32Status = am_hal_mspi_nonblocking_transfer(pFlash->pMspiHandle, &Transaction, AM_HAL_MSPI_TRANS_DMA, pfnMSPI_GD25LB512_Callback, (void *)&ui32DMAStatus);
        //
        // Check the transaction status.
        //
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        //
        // Wait for DMA Complete or Timeout
        //
        for (uint32_t i = 0; i < AM_DEVICES_MSPI_GD25LB512_TIMEOUT; i++)
        {
            if (AM_HAL_STATUS_SUCCESS == ui32DMAStatus)
            {
                break;
            }
            //
            // Call the BOOTROM cycle function to delay for about 1 microsecond.
            //
            am_util_delay_us(1);
        }
        // Check the status.
        if (AM_HAL_STATUS_SUCCESS != ui32DMAStatus)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }

        if (am_devices_mspi_gd25lb512_enter_command_mode(pHandle))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        //
        // Wait for the Write In Progress to indicate the erase is complete.
        //
        for (uint32_t i = 0; i < AM_DEVICES_MSPI_GD25LB512_TIMEOUT; i++)
        {
            // GD25LB has different number of bytes for each speed of status read.
            switch (pFlash->currDevCfg.eDeviceConfig)
            {
                case AM_HAL_MSPI_FLASH_SERIAL_CE0:
                case AM_HAL_MSPI_FLASH_SERIAL_CE1:
                case AM_HAL_MSPI_FLASH_QUAD_CE0_1_4_4:
                case AM_HAL_MSPI_FLASH_QUAD_CE1_1_4_4:
                    am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_STATUS, false, 0, false, ui32PIOBuffer, 2);
                    bWriteComplete = (0 == (ui32PIOBuffer[0] & AM_DEVICES_GD25LB512_WIP));
                    break;
                case AM_HAL_MSPI_FLASH_QUAD_CE0:
                case AM_HAL_MSPI_FLASH_QUAD_CE1:
                    am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_STATUS, false, 0, false, ui32PIOBuffer, 4);
                    bWriteComplete = (0 == ((ui32PIOBuffer[0] >> 16) & AM_DEVICES_GD25LB512_WIP));
                    break;
                default:
                    return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
            }
            am_util_delay_us(100);
            if (bWriteComplete)
            {
                break;
            }
        }
        //
        // Send the command sequence to disable writing.
        //
        ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_DISABLE, false, 0, ui32PIOBuffer, 0);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }

        if (am_devices_mspi_gd25lb512_exit_command_mode(pHandle))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }
    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

static uint32_t
mspi_gd25lb512_dma_blocking_write(void *pHandle, uint8_t *pui8TxBuffer,
                                  uint32_t ui32WriteAddress,
                                  uint32_t ui32NumBytes)
{
       am_hal_mspi_dma_transfer_t    Transaction;
    bool                          bWriteComplete = false;
    uint32_t                      ui32BytesLeft = ui32NumBytes;
    uint32_t                      ui32PageAddress = ui32WriteAddress;
    uint32_t                      ui32BufferAddress = (uint32_t)pui8TxBuffer;
    uint32_t                      ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;
    uint32_t      ui32PIOBuffer[32] = {0};
    while (ui32BytesLeft > 0)
    {
        if (am_devices_mspi_gd25lb512_enter_command_mode(pHandle))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        //
        // Send the command sequence to enable writing.
        //
        ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_ENABLE, false, 0, ui32PIOBuffer, 0);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }

        if (am_devices_mspi_gd25lb512_exit_command_mode(pHandle))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }

        //
        // Set the DMA priority
        //
        Transaction.ui8Priority = 1;
        // Set the transfer direction to TX (Write)
        Transaction.eDirection = AM_HAL_MSPI_TX;
        if (ui32BytesLeft > AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE)
        {
            //
            // Set the transfer count in bytes.
            //
            Transaction.ui32TransferCount = AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE;
            ui32BytesLeft -= AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE;
        }
        else
        {
            //
            // Set the transfer count in bytes.
            //
            Transaction.ui32TransferCount = ui32BytesLeft;
            ui32BytesLeft = 0;
        }

        // Set the address to read data to.
        Transaction.ui32DeviceAddress = ui32PageAddress;
        ui32PageAddress += AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE;

        //
        // Set the source SRAM buffer address.
        //
        Transaction.ui32SRAMAddress = ui32BufferAddress;
        ui32BufferAddress += AM_DEVICES_MSPI_GD25LB512_PAGE_SIZE;

        //
        // Clear the CQ stimulus.
        //
        Transaction.ui32PauseCondition = 0;

        //
        // Clear the post-processing
        //
        Transaction.ui32StatusSetClr = 0;
#if defined(AM_PART_APOLLO4)
        Transaction.eDeviceNum         = AM_HAL_MSPI_DEVICE0;
#endif
        //
        // Start the transaction.
        //
        volatile uint32_t ui32DMAStatus = 0xFFFFFFFF;
        ui32Status = am_hal_mspi_nonblocking_transfer(pFlash->pMspiHandle, &Transaction, AM_HAL_MSPI_TRANS_DMA, pfnMSPI_GD25LB512_Callback, (void *)&ui32DMAStatus);
        //
        // Check the transaction status.
        //
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        //
        // Wait for DMA Complete or Timeout
        //
        for (uint32_t i = 0; i < AM_DEVICES_MSPI_GD25LB512_TIMEOUT; i++)
        {
            // check DMA status without using ISR
            am_hal_mspi_interrupt_status_get(pFlash->pMspiHandle, &ui32Status, false);
            am_hal_mspi_interrupt_clear(pFlash->pMspiHandle, ui32Status);
            am_hal_mspi_interrupt_service(pFlash->pMspiHandle, ui32Status);

            if (AM_HAL_STATUS_SUCCESS == ui32DMAStatus)
            {
                break;
            }
            //
            // Call the BOOTROM cycle function to delay for about 1 microsecond.
            //
            am_util_delay_us(1);
        }
        // Check the status.
        if (AM_HAL_STATUS_SUCCESS != ui32DMAStatus)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }

        if (am_devices_mspi_gd25lb512_enter_command_mode(pHandle))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        //
        // Wait for the Write In Progress to indicate the erase is complete.
        //
        for (uint32_t i = 0; i < AM_DEVICES_MSPI_GD25LB512_TIMEOUT; i++)
        {
            // GD25LB has different number of bytes for each speed of status read.
            switch (pFlash->currDevCfg.eDeviceConfig)
            {
                case AM_HAL_MSPI_FLASH_SERIAL_CE0:
                case AM_HAL_MSPI_FLASH_SERIAL_CE1:
                case AM_HAL_MSPI_FLASH_QUAD_CE0_1_4_4:
                case AM_HAL_MSPI_FLASH_QUAD_CE1_1_4_4:
                    am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_STATUS, false, 0, false, ui32PIOBuffer, 2);
                    bWriteComplete = (0 == (ui32PIOBuffer[0] & AM_DEVICES_GD25LB512_WIP));
                    break;
                case AM_HAL_MSPI_FLASH_QUAD_CE0:
                case AM_HAL_MSPI_FLASH_QUAD_CE1:
                    am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_STATUS, false, 0, false, ui32PIOBuffer, 4);
                    bWriteComplete = (0 == ((ui32PIOBuffer[0] >> 16) & AM_DEVICES_GD25LB512_WIP));
                    break;
                default:
                    return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
            }
            am_util_delay_us(100);
            if (bWriteComplete)
            {
                break;
            }
        }
        //
        // Send the command sequence to disable writing.
        //
        ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_DISABLE, false, 0, ui32PIOBuffer, 0);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        if (am_devices_mspi_gd25lb512_exit_command_mode(pHandle))
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }
    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}
//*****************************************************************************
//
//! @brief Erases the entire contents of the external flash
//!
//! @param pHandle - Flash device handle.
//!
//! This function uses the "Bulk Erase" instruction to erase the entire
//! contents of the external flash.
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_mass_erase(void *pHandle)
{
    bool          bEraseComplete = false;
    uint32_t      ui32Status;
    uint32_t      ui32PIOBuffer[32] = {0};

    if (am_devices_mspi_gd25lb512_enter_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Send the command sequence to enable writing.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_ENABLE, false, 0, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Send the command sequence to do the mass erase.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_BULK_ERASE, false, 0, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Wait for the Write In Progress to indicate the erase is complete.
    //
    for (uint32_t i = 0; i < AM_DEVICES_MSPI_GD25LB512_ERASE_TIMEOUT; i++)
    {
        ui32PIOBuffer[0] = 0;
        am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_STATUS, false, 0, false, ui32PIOBuffer, 1);
        bEraseComplete = (0 == (ui32PIOBuffer[0] & AM_DEVICES_MSPI_GD25LB512_WIP));
        if (bEraseComplete)
        {
            break;
        }
        am_util_delay_ms(10);
    }
    //
    // Check the status.
    //
    if (!bEraseComplete)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    //
    // Send the command sequence to disable writing.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_DISABLE, false, 0, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (am_devices_mspi_gd25lb512_exit_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Erases the contents of a single sector of flash
//!
//! @param pHandle - Flash device handle.
//! @param ui32SectorAddress - Address to erase in the external flash
//!
//! This function erases a single sector of the external flash as specified by
//! ui32EraseAddress. The entire sector where ui32EraseAddress will
//! be erased.
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_sector_erase(void *pHandle, uint32_t ui32SectorAddress)
{
    bool          bEraseComplete = false;
    uint32_t      ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;
    uint32_t      ui32PIOBuffer[32] = {0};

    if (am_devices_mspi_gd25lb512_enter_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Send the command sequence to enable writing.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_ENABLE, false, 0, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Send the command to remove protection from the sector.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_GD25LB512_UNPROTECT_SECTOR, true, ui32SectorAddress, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Send the command sequence to enable writing.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_ENABLE, false, 0, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Send the command sequence to do the sector erase.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_SECTOR_ERASE, true, ui32SectorAddress, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Wait for the Write In Progress to indicate the erase is complete.
    //
    for (uint32_t i = 0; i < AM_DEVICES_MSPI_GD25LB512_ERASE_TIMEOUT; i++)
    {
        //
        // GD25LB has different number of bytes for each speed of status read.
        //
        switch (pFlash->currDevCfg.eDeviceConfig)
        {
            case AM_HAL_MSPI_FLASH_SERIAL_CE0:
            case AM_HAL_MSPI_FLASH_SERIAL_CE1:
            case AM_HAL_MSPI_FLASH_QUAD_CE0_1_4_4:
            case AM_HAL_MSPI_FLASH_QUAD_CE1_1_4_4:
                am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_STATUS, false, 0, false, ui32PIOBuffer, 2);
                bEraseComplete = (0 == (ui32PIOBuffer[0] & AM_DEVICES_GD25LB512_WIP));
                break;
            case AM_HAL_MSPI_FLASH_QUAD_CE0:
            case AM_HAL_MSPI_FLASH_QUAD_CE1:
                am_devices_mspi_gd25lb512_command_read(pHandle, AM_DEVICES_MSPI_GD25LB512_READ_STATUS, false, 0, false, ui32PIOBuffer, 4);
                bEraseComplete = (0 == ((ui32PIOBuffer[0] >> 16) & AM_DEVICES_GD25LB512_WIP));
                break;
            default:
                return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
        if (bEraseComplete)
        {
            break;
        }
        am_util_delay_ms(10);
    }

    //
    // Check the status.
    //
    if (!bEraseComplete)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Send the command sequence to disable writing.
    //
    ui32Status = am_devices_mspi_gd25lb512_command_write(pHandle, AM_DEVICES_MSPI_GD25LB512_WRITE_DISABLE, false, 0, ui32PIOBuffer, 0);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (am_devices_mspi_gd25lb512_exit_command_mode(pHandle))
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Return the status.
    //
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Sets up the MSPI into DDR mode.
//!
//! This function sets MSPI into DDR mode..
//
//! @return 32-bit status
//
//*****************************************************************************

uint32_t
am_devices_mspi_gd25lb512_enable_ddr(void *pHandle)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;
    am_hal_mspi_dev_config_t    ddrMSPICfg = pFlash->currDevCfg;

    switch (pFlash->currDevCfg.eDeviceConfig)
    {
        case AM_HAL_MSPI_FLASH_QUAD_CE0:
        case AM_HAL_MSPI_FLASH_QUAD_CE1:
            ddrMSPICfg.eInstrCfg = AM_HAL_MSPI_INSTR_2_BYTE;
            break;
        default:
            am_util_debug_printf("Error, unsupported ddr device mode\n");
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    ddrMSPICfg.bEmulateDDR   = true,
    ddrMSPICfg.ui16ReadInstr = AM_DEVICES_MSPI_GD25LB512_DDR_READ;

    ui32Status = am_devices_mspi_gd25lb512_set_dummy_cycle(pFlash, ddrMSPICfg.eClockFreq,
                                                           ddrMSPICfg.bEmulateDDR,
                                                           &ddrMSPICfg.ui8TurnAround);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (ddrMSPICfg.bEmulateDDR)
    {
        ddrMSPICfg.ui8TurnAround *= 2;
    }

    //
    // Disable MSPI defore re-configuring it.
    //
    ui32Status = am_hal_mspi_disable(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Re-Configure the MSPI for the requested operation mode.
    //
    ui32Status = am_hal_mspi_device_configure(pFlash->pMspiHandle, &ddrMSPICfg);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_DDR_EN, NULL);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

#if defined(AM_PART_APOLLO5_API)
    if (AM_HAL_MSPI_CLK_192MHZ == ddrMSPICfg.eClockFreq ||
        AM_HAL_MSPI_CLK_125MHZ == ddrMSPICfg.eClockFreq ||
        AM_HAL_MSPI_CLK_96MHZ  == ddrMSPICfg.eClockFreq)
    {
        ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_RXCFG, pFlash->pRxCfg);
    }
#elif defined(AM_PART_APOLLO4_API)
    if (AM_HAL_MSPI_CLK_96MHZ  == ddrMSPICfg.eClockFreq)
    {
        pFlash->pRxCfg->ui8RxSmp = 2;
    }

    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_RXCFG, pFlash->pRxCfg);
#endif

    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    if (pFlash->bTimingValid)
    {
        am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_TIMING_SCAN, pFlash->pTimingCfg);
    }

    //
    // Re-Enable MSPI
    //
    ui32Status = am_hal_mspi_enable(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Configure the MSPI pins.
    //
    am_bsp_mspi_pins_enable(pFlash->ui32Module, ddrMSPICfg.eDeviceConfig);

    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

uint32_t
am_devices_mspi_gd25lb512_disable_ddr(void *pHandle)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Disable MSPI defore re-configuring it.
    //
    ui32Status = am_hal_mspi_disable(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Re-Configure the MSPI for the requested operation mode.
    //
    ui32Status = am_hal_mspi_device_configure(pFlash->pMspiHandle, &pFlash->currDevCfg);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_DDR_DIS, NULL);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Re-Enable MSPI
    //
    ui32Status = am_hal_mspi_enable(pFlash->pMspiHandle);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // Configure the MSPI pins.
    //
    am_bsp_mspi_pins_enable(pFlash->ui32Module, pFlash->currDevCfg.eDeviceConfig);

    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Sets up the MSPI and external FLASH into XIP mode.
//!
//! This function sets the external psram device and the MSPI into XIP mode.
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_enable_xip(void *pHandle)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Enable XIP on the MSPI.
    //
    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_XIP_EN, NULL);

    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
#if !MSPI_USE_CQ
    // Disable the DMA interrupts.
    ui32Status = am_hal_mspi_interrupt_disable(pFlash->pMspiHandle,
                 AM_HAL_MSPI_INT_DMAERR |
                 AM_HAL_MSPI_INT_DMACMP);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
#endif
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Removes the MSPI and external FLASH from XIP mode.
//!
//! This function removes the external device and the MSPI from XIP mode.
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_disable_xip(void *pHandle)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Disable XIP on the MSPI.
    //
    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_XIP_DIS, NULL);

    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Sets up the MSPI and external FLASH into scrambling mode.
//!
//! This function sets the external psram device and the MSPI into scrambling mode.
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_enable_scrambling(void *pHandle)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Enable scrambling on the MSPI.
    //
    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_SCRAMB_EN, NULL);

    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Removes the MSPI and external FLASH from scrambling mode.
//!
//! This function removes the external device and the MSPI from scrambling mode.
//
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_disable_scrambling(void *pHandle)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    //
    // Disable Scrambling on the MSPI.
    //
    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_SCRAMB_DIS, NULL);

    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
}


#define AM_DEVICES_MSPI_GD25LB512_DATA_CHECK_FAILED  (AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR + 1)
#define AM_DEVICES_MSPI_GD25LB512_DATA_CHECK_PASS    (AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR + 2)

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

#if defined(AM_PART_APOLLO4_API)
// #### INTERNAL BEGIN ####
// FALCSW-698 9/13/23 DAXI Buffer Alignment Restrictions.
// #### INTERNAL END ####
static AM_SHARED_RW uint8_t  ui8TxBuffer[FLASH_TIMING_SCAN_SIZE_BYTES] AM_BIT_ALIGNED(128);
static AM_SHARED_RW uint8_t  ui8RxBuffer[FLASH_TIMING_SCAN_SIZE_BYTES] AM_BIT_ALIGNED(128);
#elif defined(AM_PART_APOLLO5_API)
// #### INTERNAL BEGIN ####
// a cache line is 32 bytes
// #### INTERNAL END ####
static AM_SHARED_RW uint8_t  ui8TxBuffer[FLASH_TIMING_SCAN_SIZE_BYTES] __attribute__((aligned(32)));
static AM_SHARED_RW uint8_t  ui8RxBuffer[FLASH_TIMING_SCAN_SIZE_BYTES] __attribute__((aligned(32)));
#else
static AM_SHARED_RW uint8_t  ui8TxBuffer[FLASH_TIMING_SCAN_SIZE_BYTES];
static AM_SHARED_RW uint8_t  ui8RxBuffer[FLASH_TIMING_SCAN_SIZE_BYTES];
#endif

static bool
flash_write(void* flashHandle, uint32_t length)
{
    //
    // Try to use as less ram as possible in stack
    //
    uint32_t ui32NumberOfBytesLeft = length;
    uint32_t ui32TestBytes = 0;
    uint32_t ui32AddressOffset = 0;
    uint8_t  ui8PatternCounter = 0;
    uint8_t  *pui8TxBuffer = ui8TxBuffer;

    uint32_t ui32Status = AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;

    while ( ui32NumberOfBytesLeft )
    {
        if ( ui32NumberOfBytesLeft > FLASH_CHECK_DATA_SIZE_BYTES )
        {
            ui32TestBytes = FLASH_CHECK_DATA_SIZE_BYTES;
            ui32NumberOfBytesLeft -= FLASH_CHECK_DATA_SIZE_BYTES;
        }
        else
        {
            ui32TestBytes = ui32NumberOfBytesLeft;
            ui32NumberOfBytesLeft = 0;
        }

        //
        // Write to target address with test pattern with given length
        // Use 5 patterns: 0x5555AAAA, 0xFFFF0000, Walking, incremental and decremental
        //
        prepare_test_pattern((ui8PatternCounter) % FLASH_TEST_PATTERN_NUMBER, pui8TxBuffer, ui32TestBytes);
#if defined(AM_PART_APOLLO5_API)
        am_hal_cachectrl_range_t sRange =
        {
            .ui32StartAddr = (uint32_t)pui8TxBuffer,
            .ui32Size = ui32TestBytes,
        };
        am_hal_cachectrl_dcache_clean(&sRange);
#endif
        ui8PatternCounter++;

        //
        // write to target address
        //
        ui32Status = mspi_gd25lb512_dma_blocking_write(flashHandle, pui8TxBuffer,
                                            (AM_DEVICES_MSPI_GD25LB512_SECTOR_FOR_TIMING_CHECK << AM_DEVICES_MSPI_FLASH_SECTOR_SHIFT) + ui32AddressOffset,
                                            ui32TestBytes);
        if ( ui32Status ==  AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR)
        {
            return true;
        }

        ui32AddressOffset += ui32TestBytes;
        pui8TxBuffer += ui32TestBytes;
    }

    return false;
}

static uint32_t
flash_check(void* flashHandle, uint32_t length)
{
    uint32_t ui32Status = AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;

    //
    // Read back data
    //
#if defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO5_API)
    ui32Status = mspi_gd25lb512_dma_blocking_read(flashHandle, ui8RxBuffer,
                                                  AM_DEVICES_MSPI_GD25LB512_SECTOR_FOR_TIMING_CHECK << AM_DEVICES_MSPI_FLASH_SECTOR_SHIFT,
                                                  length);
#else
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)flashHandle;
    if ( pFlash->currDevCfg.eClockFreq == AM_HAL_MSPI_CLK_96MHZ )
    {
        //
        // Read the data back into the RX buffer using XIP
        //
        ui32Status = am_devices_mspi_gd25lb512_enable_xip(flashHandle);
        if (AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS != ui32Status)
        {
            am_util_debug_printf("Failed to put the MSPI into XIP mode!\n");
        }
        am_hal_sysctrl_bus_write_flush();
        uint8_t * xipPointer = (uint8_t *)(ui32MspiXipBaseAddress[pFlash->ui32Module] + (AM_DEVICES_MSPI_GD25LB512_SECTOR_FOR_TIMING_CHECK << AM_DEVICES_MSPI_FLASH_SECTOR_SHIFT));
        memcpy((uint8_t*)ui8RxBuffer, xipPointer, length);

        //
        // Quit XIP mode
        //
        ui32Status = mspi_gd25lb512_disable_xip(flashHandle);
        if (AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS != ui32Status)
        {
            am_util_debug_printf("Failed to disable XIP mode in the MSPI!\n");
        }
    }
    else
    {
        ui32Status = mspi_gd25lb512_dma_blocking_read(flashHandle, ui8RxBuffer,
                                                      AM_DEVICES_MSPI_GD25LB512_SECTOR_FOR_TIMING_CHECK << AM_DEVICES_MSPI_FLASH_SECTOR_SHIFT,
                                                      length);
    }
#endif

    if ( ui32Status ==  AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR)
    {
        return ui32Status;
    }

    //
    // Verify the result
    //
    if ( memcmp(ui8RxBuffer, ui8TxBuffer, length) )
    {
        //
        // verify failed, return directly
        //
        return AM_DEVICES_MSPI_GD25LB512_DATA_CHECK_FAILED;
    }

    return AM_DEVICES_MSPI_GD25LB512_DATA_CHECK_PASS;
}

//
// Static helper function:
//  Count the longest consecutive 1s in a 32bit word
//
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

//
// Static helper function:
//  Find and return the mid point of the longest continuous 1s in a 32bit word
//
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

#if defined(AM_PART_APOLLO4) || defined(AM_PART_APOLLO4B)
//*****************************************************************************
//
//! @brief Checks Flash timing and determine a delay setting.
//!
//! @param pDeviceID - Pointer to the return buffer for the Device ID.
//!
//! This function scans through the delay settings of MSPI DDR mode and selects
//! the best parameter to use by tuning TURNAROUND/RXNEG/RXDQSDELAY0 values.
//! This function is only valid in DDR mode and ENABLEDQS0 = 0.
//!
//! @return 32-bit status, scan result in structure type
//
//*****************************************************************************
const am_devices_mspi_gd25lb512_timing_config_t gd25lb512_sConfigArray[8] =
{
    {0, 0, 0, 0, 1, 5}, // Turnaround=5 , RXNEG=0, RXDQSDELAY=Dummy
    {0, 1, 0, 0, 1, 5}, // Turnaround=5 , RXNEG=1, RXDQSDELAY=Dummy
    {0, 0, 0, 0, 1, 6}, // Turnaround=6 , RXNEG=0, RXDQSDELAY=Dummy
    {0, 1, 0, 0, 1, 6}, // Turnaround=6 , RXNEG=1, RXDQSDELAY=Dummy
    {0, 0, 0, 0, 1, 7}, // Turnaround=7 , RXNEG=0, RXDQSDELAY=Dummy
    {0, 1, 0, 0, 1, 7}, // Turnaround=7 , RXNEG=1, RXDQSDELAY=Dummy
    {0, 0, 0, 0, 1, 8}, // Turnaround=8 , RXNEG=0, RXDQSDELAY=Dummy
    {0, 1, 0, 0, 1, 8}, // Turnaround=8 , RXNEG=1, RXDQSDELAY=Dummy
};

uint32_t
am_devices_mspi_gd25lb512_init_timing_check(uint32_t module,
                                            am_devices_mspi_gd25lb512_config_t *pDevCfg,
                                            am_devices_mspi_gd25lb512_timing_config_t *pDevSdrCfg)
{
    uint32_t ui32Status;
    void *pDevHandle;
    void *pHandle;
    uint32_t ui32ResultArray[8] =
    {
        0,  // Turnaround = 5 , RXNEG = 0
        0,  // Turnaround = 5 , RXNEG = 1
        0,  // Turnaround = 6 , RXNEG = 0
        0,  // Turnaround = 6 , RXNEG = 1
        0,  // Turnaround = 7 , RXNEG = 0
        0,  // Turnaround = 7 , RXNEG = 1
        0,  // Turnaround = 8 , RXNEG = 0
        0,  // Turnaround = 8 , RXNEG = 1
    };

    am_hal_mspi_dqs_t scanCfg =
    {
        .bDQSEnable             = 0,
        .bEnableFineDelay       = 1,
        .bOverrideRXDQSDelay    = 1,
        .ui8RxDQSDelay          = 15,
        .bOverrideTXDQSDelay    = 0,
        .ui8TxDQSDelay          = 0,
        .bDQSSyncNeg            = 0,
        .ui8DQSDelay            = 0,
        .ui8PioTurnaround       = 8,
        .ui8XipTurnaround       = 8,
        .bRxNeg                 = 0,
    };

    // clear previous saved config, rescan
    if ( bSDRTimingConfigSaved == true )
    {
        bSDRTimingConfigSaved              = false;
        TimingConfigStored.ui8RxDQSDelay   = TimingConfigDefault.ui8RxDQSDelay;
        TimingConfigStored.bRxNeg          = TimingConfigDefault.bRxNeg;
        TimingConfigStored.ui8Turnaround   = TimingConfigDefault.ui8Turnaround;
    }

    //
    // initialize interface
    //
    ui32Status = am_devices_mspi_gd25lb512_init(module, pDevCfg, &pDevHandle, &pHandle);
    if (AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("    Failed to configure the MSPI and Flash Device correctly!\n");
        return ui32Status;
    }

    //
    // erase target sector first (each "sector is 64Kbyte block")
    //
    if ( FLASH_TIMING_SCAN_SIZE_BYTES % AM_DEVICES_MSPI_GD25LB512_SECTOR_SIZE )
    {
        // scan size shall be at block boundary
        am_util_debug_printf("ERROR: Timing scan data size shall be at sector boundary!\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    for ( uint8_t i = 0; i < (FLASH_TIMING_SCAN_SIZE_BYTES / AM_DEVICES_MSPI_GD25LB512_SECTOR_SIZE); i++ )
    {
        ui32Status = am_devices_mspi_gd25lb512_sector_erase(pDevHandle,
                                                        (AM_DEVICES_MSPI_GD25LB512_SECTOR_FOR_TIMING_CHECK << AM_DEVICES_MSPI_FLASH_SECTOR_SHIFT) + i*AM_DEVICES_MSPI_GD25LB512_SECTOR_SIZE);

        if (AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS != ui32Status)
        {
            am_util_debug_printf("Failed to erase Flash Device sector!\n");
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }

    // write test pattern into target sector
    if ( flash_write(pDevHandle, FLASH_TIMING_SCAN_SIZE_BYTES) )
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    // Run data check first without set Turnaround and RXNEG
    ui32Status = flash_check(pDevHandle, FLASH_TIMING_SCAN_SIZE_BYTES);
    if (ui32Status == AM_DEVICES_MSPI_GD25LB512_DATA_CHECK_PASS)
    {
        //
        // Data check pass ,Deinitialize the MSPI interface
        //
        am_devices_mspi_gd25lb512_deinit(pDevHandle);
        pDevSdrCfg->ui8RxDQSDelay = 0;
        pDevSdrCfg->bRxNeg = 0;
        pDevSdrCfg->ui8Turnaround = 0;
        return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    }

    //
    // Start timing scan .
    //
    for ( uint8_t i = 0; i < 8; i++ )
    {
        // set Turnaround and RXNEG
        scanCfg.ui8PioTurnaround    = scanCfg.ui8XipTurnaround = gd25lb512_sConfigArray[i].ui8Turnaround;
        scanCfg.bRxNeg              = gd25lb512_sConfigArray[i].bRxNeg;
        for ( uint8_t RxDqs_Index = 1; RxDqs_Index < 31; RxDqs_Index++ )
        {
            // set RXDQSDELAY0 value
            scanCfg.ui8RxDQSDelay   = RxDqs_Index;
            // apply settings
            ui32Status = am_hal_mspi_control(pHandle, AM_HAL_MSPI_REQ_DQS, &scanCfg);
            if (AM_HAL_STATUS_SUCCESS != ui32Status)
            {
                return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
            }

            // run data check
            ui32Status = flash_check(pDevHandle, FLASH_TIMING_SCAN_SIZE_BYTES);
            if (ui32Status == AM_DEVICES_MSPI_GD25LB512_DATA_CHECK_PASS)
            {
                // data check pass
                ui32ResultArray[i] |= 0x01 << RxDqs_Index;
            }
            else if (ui32Status == AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR)
            {
                am_devices_mspi_gd25lb512_deinit(pDevHandle);

                ui32Status = am_devices_mspi_gd25lb512_init(module, pDevCfg, &pDevHandle, &pHandle);
                if (AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS != ui32Status)
                {
                    am_util_debug_printf("    Failed to configure the MSPI and Flash Device correctly!\n");
                    return ui32Status;
                }
            }
            else
            {
                // data check failed

            }
        }
    }

    //
    // Check result
    //
    uint32_t ui32MaxOnesIndex = 0;
    uint32_t ui32MaxOnes = 0;
    uint32_t ui32Result = 0;
    for ( uint32_t i = 0; i < 8; i++ )
    {
        ui32Result = count_consecutive_ones(&ui32ResultArray[i]);
        if ( ui32Result > ui32MaxOnes )
        {
            ui32MaxOnes = ui32Result;
            ui32MaxOnesIndex = i;
        }

        //
        // print result for test
        //
        am_util_debug_printf("    Setting %d = 0x%08X\n", i, ui32ResultArray[i]);
    }
    am_util_debug_printf("Timing Scan found a window %d fine steps wide.\n", ui32MaxOnes);

    //
    // Find RXDQSDELAY Value
    //
    uint32_t dqsdelay = find_mid_point(&ui32ResultArray[ui32MaxOnesIndex]);

    //
    // Deinitialize the MSPI interface
    //
    am_devices_mspi_gd25lb512_deinit(pDevHandle);

    //
    // Check consecutive passing settings
    //
    if ( ui32MaxOnes < FLASH_TIMING_SCAN_MIN_ACCEPTANCE_LENGTH )
    {
        // too short is the passing settings, use default setting
        pDevSdrCfg->ui8RxDQSDelay = TimingConfigDefault.ui8RxDQSDelay;
        pDevSdrCfg->bRxNeg = TimingConfigDefault.bRxNeg;
        pDevSdrCfg->ui8Turnaround = TimingConfigDefault.ui8Turnaround;
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }
    else
    {
        //
        // Set output values
        //
        pDevSdrCfg->ui8RxDQSDelay = dqsdelay;
        pDevSdrCfg->bRxNeg = gd25lb512_sConfigArray[ui32MaxOnesIndex].bRxNeg;
        pDevSdrCfg->ui8Turnaround = gd25lb512_sConfigArray[ui32MaxOnesIndex].ui8Turnaround;

        return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    }
}

//*****************************************************************************
//
//! @brief Apply given SDR timing settings to target MSPI instance.
//!
//! @param pHandle - Handle to the flash.
//! @param pDevSdrCfg - Pointer to the ddr timing config structure
//!
//! This function applies the ddr timing settings to the selected mspi instance.
//! This function must be called after MSPI instance is initialized into
//! ENABLEFINEDELAY0 = 1 mode.
//!
//! @return 32-bit status
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_apply_timing(void *pHandle,
                                       am_devices_mspi_gd25lb512_timing_config_t *pDevSdrCfg)
{
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;
    am_hal_mspi_dqs_t applyCfg =
    {
        .bDQSEnable             = 0,
        .bEnableFineDelay       = 1,
        .bOverrideRXDQSDelay    = 1,
        .bOverrideTXDQSDelay    = 0,
        .ui8TxDQSDelay          = 0,
        .bDQSSyncNeg            = 0,
        .ui8DQSDelay            = 0,
    };

    if ( (pDevSdrCfg->ui8RxDQSDelay == 0) && (pDevSdrCfg->bRxNeg == 0) &&   \
         (pDevSdrCfg->ui8Turnaround == 0) )
    {
       return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
    }


    // apply timing settings: Turnaround, RXNEG and RXDQSDELAY
    applyCfg.ui8RxDQSDelay      = pDevSdrCfg->ui8RxDQSDelay;
    applyCfg.ui8PioTurnaround   = pDevSdrCfg->ui8Turnaround;
    applyCfg.ui8XipTurnaround   = pDevSdrCfg->ui8Turnaround;
    applyCfg.bRxNeg             = pDevSdrCfg->bRxNeg;

    // save a local copy of the timing settings
    if ( bSDRTimingConfigSaved == false )
    {
        bSDRTimingConfigSaved               = true;
        TimingConfigStored.ui8RxDQSDelay    = pDevSdrCfg->ui8RxDQSDelay;
        TimingConfigStored.bRxNeg           = pDevSdrCfg->bRxNeg;
        TimingConfigStored.ui8Turnaround    = pDevSdrCfg->ui8Turnaround;
    }

    return am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_DQS, &applyCfg);

}

#elif defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO5_API)

//#define SCAN_TXNEG
//#define SCAN_RXNEG
//#define SCAN_RXCAP
//#define SCAN_TURNAROUND
#define SCAN_TXDQSDELAY
#define SCAN_RXDQSDELAY
//*****************************************************************************
//
// Timing scan entry of the mspi_gd25lb512 driver
//
//*****************************************************************************
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
#define SCAN_TURNAROUND_START -1
#define SCAN_TURNAROUND_END   0
#endif

#if defined(SCAN_TXDQSDELAY)
#define SCAN_TXDQSDELAY_START 0
#define SCAN_TXDQSDELAY_END   7
#endif

#if defined(SCAN_RXDQSDELAY)
#define SCAN_RXDQSDELAY_START 0
#define SCAN_RXDQSDELAY_END   31
#endif

uint32_t
am_devices_mspi_gd25lb512_init_timing_check(uint32_t ui32Module,
                                            am_devices_mspi_gd25lb512_config_t *pDevCfg,
                                            am_devices_mspi_gd25lb512_timing_config_t *pDevTimingCfg)
{
    uint32_t ui32Status;
    am_devices_mspi_gd25lb512_t *pFlash;
    void *pHandle;
    uint32_t Txdqsdelay = 0;
    uint32_t Rxdqsdelay = 0;

    uint32_t ui32CCOResult = 0;
    uint32_t ui32TxResult = 0;
    uint32_t ui32RxResultArray[32];

    am_hal_mspi_timing_scan_t scanCfg;

    //
    // initialize interface
    //
    ui32Status = am_devices_mspi_gd25lb512_init(ui32Module, pDevCfg, (void *)&pFlash, &pHandle);
    if (AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("    Failed to configure the MSPI and Flash Device correctly!\n");
        return ui32Status;
    }

#if defined(FAST_TIMING_SCAN)
    bool bTimingValid = pFlash->bTimingValid;
#endif
    pFlash->bTimingValid = false;

    //
    // erase target sector first (each "sector is 4Kbyte block")
    //
    if ( FLASH_TIMING_SCAN_SIZE_BYTES % AM_DEVICES_MSPI_GD25LB512_SECTOR_SIZE )
    {
        // scan size shall be at block boundary
        am_util_debug_printf("ERROR: Timing scan data size shall be at sector boundary!\n");
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    for ( uint8_t i = 0; i < (FLASH_TIMING_SCAN_SIZE_BYTES / AM_DEVICES_MSPI_GD25LB512_SECTOR_SIZE); i++ )
    {
        ui32Status = am_devices_mspi_gd25lb512_sector_erase(pFlash,
                                                        (AM_DEVICES_MSPI_GD25LB512_SECTOR_FOR_TIMING_CHECK << AM_DEVICES_MSPI_FLASH_SECTOR_SHIFT) + i*AM_DEVICES_MSPI_GD25LB512_SECTOR_SIZE);

        if (AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS != ui32Status)
        {
            am_util_debug_printf("Failed to erase Flash Device sector!\n");
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }
    }

    //
    // write test pattern into target sector
    //
    if ( flash_write(pFlash, FLASH_TIMING_SCAN_SIZE_BYTES) )
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

#if defined(AM_DEVICES_MSPI_GD25LB512_ENABLE_DDR_READ)
    ui32Status = am_devices_mspi_gd25lb512_enable_ddr(pFlash);
    if (AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS != ui32Status)
    {
        am_util_debug_printf("Failed to disable DDR mode!\n");
    }
#endif

    //
    //get timing scan param configured by init & hal
    //
    ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_TIMING_SCAN_GET, &scanCfg);
    if (AM_HAL_STATUS_SUCCESS != ui32Status)
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

#if defined(FAST_TIMING_SCAN)
    if ( bTimingValid )
    {
        //
        // apply settings
        //
        ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_TIMING_SCAN, pFlash->pTimingCfg);
        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
        }

        //
        // run data check
        //
        if ( AM_DEVICES_MSPI_GD25LB512_DATA_CHECK_PASS == flash_check(pFlash, FLASH_TIMING_SCAN_SIZE_BYTES) )
        {
            pDevTimingCfg->bTxNeg          = pFlash->pTimingCfg->bTxNeg;
            pDevTimingCfg->bRxNeg          = pFlash->pTimingCfg->bRxNeg;
            pDevTimingCfg->bRxCap          = pFlash->pTimingCfg->bRxCap;
            pDevTimingCfg->ui8Turnaround   = pFlash->pTimingCfg->ui8Turnaround;
            pDevTimingCfg->ui8TxDQSDelay   = pFlash->pTimingCfg->ui8TxDQSDelay;
            pDevTimingCfg->ui8RxDQSDelay   = pFlash->pTimingCfg->ui8RxDQSDelay;
            //
            // Deinitialize the MSPI interface
            //
            am_devices_mspi_gd25lb512_deinit(pFlash);
            am_util_debug_printf("Skipping timing scan.\n");
            return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
        }
    }
#endif

    am_util_debug_printf("\nStart MSPI Timing Scan!\n");

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
                for ( scanCfg.ui8Turnaround = ui32Turnaround + SCAN_TURNAROUND_START; scanCfg.ui8Turnaround <= ui32Turnaround + SCAN_TURNAROUND_END; scanCfg.ui8Turnaround++ )
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
                            ui32Status = am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_TIMING_SCAN, &scanCfg);
                            if (AM_HAL_STATUS_SUCCESS != ui32Status)
                            {
                                return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
                            }

                            //
                            // run data check
                            //
                            if ( AM_DEVICES_MSPI_GD25LB512_DATA_CHECK_PASS == flash_check(pFlash, FLASH_TIMING_SCAN_SIZE_BYTES) )
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
                        if ( ui32CCOResult > FLASH_TIMING_SCAN_MIN_ACCEPTANCE_LENGTH )
                        {
                            ui32TxResult |= 0x01 << scanCfg.ui8TxDQSDelay;
                        }
                        am_util_debug_printf("    TxDQSDelay: %d, RxDQSDelay Scan = 0x%08X, Window Size = %d\n", scanCfg.ui8TxDQSDelay, ui32RxResultArray[scanCfg.ui8TxDQSDelay], ui32CCOResult);
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
                        am_devices_mspi_gd25lb512_deinit(pFlash);
                        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
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

                    //
                    // Set output values
                    //
                    pDevTimingCfg->bTxNeg          = scanCfg.bTxNeg;
                    pDevTimingCfg->bRxNeg          = scanCfg.bRxNeg;
                    pDevTimingCfg->bRxCap          = scanCfg.bRxCap;
                    pDevTimingCfg->ui8Turnaround   = scanCfg.ui8Turnaround;
                    pDevTimingCfg->ui8TxDQSDelay   = scanCfg.ui8TxDQSDelay;
                    pDevTimingCfg->ui8RxDQSDelay   = scanCfg.ui8RxDQSDelay;

                    am_devices_mspi_gd25lb512_deinit(pFlash);
                    return AM_DEVICES_MSPI_GD25LB512_STATUS_SUCCESS;
                }
            }
        }
    }
    am_util_debug_printf("Timing Scan found no window!\n");
    //
    // Deinitialize the MSPI interface
    //
    am_devices_mspi_gd25lb512_deinit(pFlash);
    return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
}

//*****************************************************************************
//
// Apply given SDR timing settings entry of the mspi_gd25lb512 driver
//
//*****************************************************************************
uint32_t
am_devices_mspi_gd25lb512_apply_timing(void *pHandle,
                                       am_devices_mspi_gd25lb512_timing_config_t *pDevTimingCfg)
{
    am_devices_mspi_gd25lb512_t *pFlash = (am_devices_mspi_gd25lb512_t *)pHandle;

    if ( pHandle == NULL )
    {
        return AM_DEVICES_MSPI_GD25LB512_STATUS_ERROR;
    }

    //
    // apply timing setting
    //
    pFlash->pTimingCfg->bTxNeg                = pDevTimingCfg->bTxNeg;
    pFlash->pTimingCfg->bRxNeg                = pDevTimingCfg->bRxNeg;
    pFlash->pTimingCfg->bRxCap                = pDevTimingCfg->bRxCap;
    pFlash->pTimingCfg->ui8TxDQSDelay         = pDevTimingCfg->ui8TxDQSDelay;
    pFlash->pTimingCfg->ui8RxDQSDelay         = pDevTimingCfg->ui8RxDQSDelay;
    pFlash->pTimingCfg->ui8Turnaround         = pDevTimingCfg->ui8Turnaround;
    pFlash->bTimingValid                      = true;

    am_util_debug_printf("Following MSPI timing setting is applied.\n");
    am_util_debug_printf("TxNeg       = %d\n",   pDevTimingCfg->bTxNeg);
    am_util_debug_printf("RxNeg       = %d\n",   pDevTimingCfg->bRxNeg);
    am_util_debug_printf("RxCap       = %d\n",   pDevTimingCfg->bRxCap);
    am_util_debug_printf("Turnaround  = %d\n",   pDevTimingCfg->ui8Turnaround);
    am_util_debug_printf("TxDQSDelay  = %d\n",   pDevTimingCfg->ui8TxDQSDelay);
    am_util_debug_printf("RxDQSDelay  = %d\n\n", pDevTimingCfg->ui8RxDQSDelay);

    return am_hal_mspi_control(pFlash->pMspiHandle, AM_HAL_MSPI_REQ_TIMING_SCAN, pFlash->pTimingCfg);

}

#endif
