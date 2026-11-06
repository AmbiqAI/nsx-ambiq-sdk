//*****************************************************************************
//
//! @file am_hal_xspimc.h
//!
//! @brief High level functions for interfacing with the xSPI.
//!
//! @addtogroup xspimc_at110 xSPI - Multi-bit SPI
//! @ingroup atomiq110_hal
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
#ifndef AM_HAL_XSPIMC_H
#define AM_HAL_XSPIMC_H

#include "am_hal_periph.h"
#include "am_hal_sysctrl.h"
#include "am_hal_xspimc_ll.h"

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @brief xSPI Global macros
//! @defgroup xspimc_macro_ap6xx
//! @ingroup ap6xx_hal
//! @{
//
//*****************************************************************************

// #### INTERNAL BEGIN ####
#define AM_HAL_XSPIMC_ENABLE_CLKMGMT 1

#ifdef FPGA_FREQ_RATIO
#define AM_HAL_XSPIMC_TIMEOUT_MAX           (1000000 * FPGA_FREQ_RATIO)
#else
// #### INTERNAL END ####
#define AM_HAL_XSPIMC_TIMEOUT_MAX           1000000
// #### INTERNAL BEGIN ####
#endif
// #### INTERNAL END ####

#define AM_HAL_XSPIMC_RAW_RAM_SIZE          680
#define AM_HAL_XSPIMC_GLOBAL_RAM_SIZE      \
        (AM_HAL_XSPIMC_RAW_RAM_SIZE + 2u * AM_HAL_PERIPH_TOKEN_CB_SIZE)

//! @} END_GROUP - xSPI Global macros

//*****************************************************************************
//
//! @brief xSPI public types
//! @defgroup xspimc_types_at110
//! @ingroup xspimc_at110
//! @{
//
//*****************************************************************************
typedef enum
{
    AM_HAL_XSPIMC_PHYCLK_INVALID = 0,
    AM_HAL_XSPIMC_PHYCLK_PLL,
    AM_HAL_XSPIMC_PHYCLK_HFRC,
    AM_HAL_XSPIMC_PHYCLK_HFRC2,
} am_hal_xspimc_phyclk_src_e;

typedef enum
{
    AM_HAL_XSPIMC_HARDWARE_CE = 0,
    AM_HAL_XSPIMC_GPIO_CE,
} am_hal_xspimc_ce_mode_e;

typedef enum
{
    AM_HAL_XSPIMC_IO_SERIAL = 0,
    AM_HAL_XSPIMC_IO_1_1_2,
    AM_HAL_XSPIMC_IO_1_2_2,
    AM_HAL_XSPIMC_IO_DUAL,
    AM_HAL_XSPIMC_IO_1_1_4,
    AM_HAL_XSPIMC_IO_1_4_4,
    AM_HAL_XSPIMC_IO_QUAD,
    AM_HAL_XSPIMC_IO_1_1_8,
    AM_HAL_XSPIMC_IO_1_8_8,
    AM_HAL_XSPIMC_IO_OCTAL,
    AM_HAL_XSPIMC_IO_8_8_16,
    AM_HAL_XSPIMC_IO_8_16_16,
    AM_HAL_XSPIMC_IO_HEX,
} am_hal_xspimc_io_mode_e;

typedef enum
{
    AM_HAL_XSPIMC_DATA_RATE_SINGLE = 0,
    AM_HAL_XSPIMC_DATA_RATE_S_S_D,
    AM_HAL_XSPIMC_DATA_RATE_S_D_D,
    AM_HAL_XSPIMC_DATA_RATE_DUAL,
} am_hal_xspimc_data_rate_e;

typedef enum
{
    AM_HAL_XSPIMC_CPP_MODE_0 = 0,
    AM_HAL_XSPIMC_CPP_MODE_1,
    AM_HAL_XSPIMC_CPP_MODE_2,
    AM_HAL_XSPIMC_CPP_MODE_3,
} am_hal_xspimc_cpp_mode_e;

typedef enum
{
    AM_HAL_XSPIMC_PSRAM_DEVICE = 0,
    AM_HAL_XSPIMC_FLASH_DEVICE,
    AM_HAL_XSPIMC_STACKED_DEVICE,
    AM_HAL_XSPIMC_NONMEM_DEVICE,
    AM_HAL_XSPIMC_APM_HEX_PSRAM_DEVICE,
    AM_HAL_XSPIMC_HYPERRAM_PSRAM_DEVICE,
} am_hal_xspimc_dev_type_e;

typedef enum
{
    //! Configure an AIP window (address range, permissions, prefetch, enable).
    AM_HAL_XSPIMC_CONTROL_AIP_CONFIG = 0,
    //! Configure automatic Low power entry/exit for an AIP window (XIP).
    AM_HAL_XSPIMC_CONTROL_AUTO_POWER_CONFIG,
    //! Configure read-while-write behavior (flash read access during write/erase).
    AM_HAL_XSPIMC_CONTROL_READ_WHILE_WRITE_CONFIG,
    //! Configure timing-related parameters.
    AM_HAL_XSPIMC_CONTROL_TIMING_CONFIG,
    //! Register callback(s) for driver events.
    AM_HAL_XSPIMC_CONTROL_REGISTER_CALLBACK,
    //! Forward a miscellaneous control operation to the middle-level
    //! am_hal_xspimc_misc_control() API. pConfig points to an
    //! am_hal_xspimc_misc_control_e value selecting the operation.
    AM_HAL_XSPIMC_CONTROL_MISC_CONTROL,
} am_hal_xspimc_control_e;

//!
//! @brief Miscellaneous middle-level control operations.
//!
//! These operations are forwarded by the high-level
//! AM_HAL_XSPIMC_CONTROL_MISC_CONTROL control and may also be invoked
//! directly by other high-level control handlers. They use a separate
//! AM_HAL_XSPIMC_MISC_ prefix to distinguish the middle-level operation
//! space from the high-level AM_HAL_XSPIMC_CONTROL_ space.
//!
typedef enum
{
    //! Activate the read-while-write AIP holdoff before sending erasae/program
    //! command through the custom SPI interface. i.e. PIO/DMA_XFER
    AM_HAL_XSPIMC_MISC_RWW_HOLDOFF_ACTIVATE = 0,
    //! Deactivate the read-while-write AIP holdoff
    AM_HAL_XSPIMC_MISC_RWW_HOLDOFF_DEACTIVATE,
    //! Cancel an in-progress read-while-write holdoff for the device's AIP
    //! windows by writing AIPHOLDOFFCTRL. This does not disable suspend/resume
    //! or alter the holdoff-select mask; the RWW configuration stays armed.
    AM_HAL_XSPIMC_MISC_RWW_HOLDOFF_CANCEL,
    AM_HAL_XSPIMC_MISC_CTRL_MAX,
} am_hal_xspimc_misc_control_e;

//!
//! @brief Extended miscellaneous middle-level only control operations.
//!
//! These operations are NOT forwarded by the high-level
//! AM_HAL_XSPIMC_CONTROL_MISC_CONTROL control.
typedef enum
{
    //! Configure the WP pin control.
    AM_HAL_XSPIMC_MISC_WP_CTRL = AM_HAL_XSPIMC_MISC_CTRL_MAX,
    //! Configure the Hold pin control.
    AM_HAL_XSPIMC_MISC_HOLD_CTRL,
    //! Configure the custom SPI sample count (uint32_t, 1..256).
    AM_HAL_XSPIMC_MISC_SAMPLE_POINT,
    //! Configure custom hold time.
    AM_HAL_XSPIMC_MISC_HOLD_TIME,
} am_hal_xspimc_misc_control_ee;

typedef enum
{
    AM_HAL_XSPIMC_DMA_COMPLETE_CB = 0,
    AM_HAL_XSPIMC_DMA_IDLE_CB,
    AM_HAL_XSPIMC_DMA_ERROR_CB,
    AM_HAL_XSPIMC_CB_MAX,
} am_hal_xspimc_callback_type_e;

typedef enum
{
    AM_HAL_XSPIMC_PIO_XFER,
    AM_HAL_XSPIMC_DMA_XFER,
} am_hal_xspimc_xfer_mode_t;

typedef enum
{
    AM_HAL_XSPIMC_XFER_WRITE = AM_HAL_XSPIMC_LL_DATA_WRITE,
    AM_HAL_XSPIMC_XFER_READ = AM_HAL_XSPIMC_LL_DATA_READ,
} am_hal_xspimc_xfer_dir_e;

typedef enum
{
    AM_HAL_XSPIMC_AIP_RO = 0,
    AM_HAL_XSPIMC_AIP_RW,
} am_hal_xspimc_aip_permit_e;

typedef enum
{
    AM_HAL_XSPIMC_EVENT_NO_CB = 0,
    AM_HAL_XSPIMC_EVENT_AIP_ERROR_CB = 1,
    AM_HAL_XSPIMC_EVENT_DMA_ERROR_CB = 2,
    AM_HAL_XSPIMC_EVENT_XFER_COMPLETE_CB = 4,
    AM_HAL_XSPIMC_EVENT_TIMEOUT_CB = 8,
} am_hal_xspimc_event_cb_e;

typedef enum
{
    AM_HAL_XSPIMC_EVENT_ERROR = 0,
    AM_HAL_XSPIMC_EVENT_XFER_COMPLETE = 1,
    AM_HAL_XSPIMC_EVENT_TIMEOUT = 2,
} am_hal_xspimc_event_e;

typedef enum
{
    AM_HAL_XSPIMC_POWER_REQUEST_ON,
    AM_HAL_XSPIMC_POWER_REQUEST_OFF,
} am_hal_xspimc_power_req_e;

typedef struct
{
    bool     bAutoConfig;
    bool     bEncrypt;
    bool     bAutoLowPower;
    bool     bDMA;
    bool     bAIP;
    uint8_t  ui8NumCEs;
    uint8_t  ui8NumGPIOs;
    uint8_t  ui8DataWidth;
    uint8_t  ui8ClockOptions;
    uint8_t  ui8AIPType;
    uint8_t  ui8AIPAddrWidth;
    uint8_t  ui8AIPIDWidth;
    uint8_t  ui8FIFODepth;
    uint16_t ui16DelayTapNum;
    uint8_t  ui8DMAAddrWidth;
    uint8_t  ui8NumAIPWindows;
} am_hal_xspimc_capability_t;

typedef struct
{
    void *pCLKMGR;
} am_hal_xspimc_deps_t;

typedef struct
{
    uint8_t ui8ErrorStat;
    bool    bBusy;
    bool    bEnabled;
} am_hal_xspimc_status_t;

typedef struct
{
    //! Current physical controller power state.
    am_hal_sysctrl_power_state_e ePowerState;
    //! Number of device slots with valid configuration.
    uint8_t                      ui8ConfiguredDevices;
    //! Number of device slots that demand controller power.
    uint8_t                      ui8PowerDemandDevices;
    //! True after the controller has been configured.
    bool                         bConfigured;
} am_hal_xspimc_config_status_t;

typedef enum
{
    // Summary controller status (am_hal_xspimc_status_t)
    AM_HAL_XSPIMC_STATUS_TYPE_CONTROLLER = 0,
    // Raw composed interrupt/status bitfield (uint32_t), independent of IRQ enables.
    AM_HAL_XSPIMC_STATUS_TYPE_IRQ_RAW,
    // Composed interrupt/status bitfield filtered to enabled interrupts only (uint32_t).
    AM_HAL_XSPIMC_STATUS_TYPE_IRQ_ENABLED_ONLY,
    // Transfer-context status (am_hal_xspimc_xfer_ctx_status_t)
    AM_HAL_XSPIMC_STATUS_TYPE_XFER_CONTEXT,
    // Controller and device configuration status (am_hal_xspimc_config_status_t)
    AM_HAL_XSPIMC_STATUS_TYPE_CONFIGURATION,
} am_hal_xspimc_status_type_e;

typedef struct
{
    bool                      bActive;
    bool                      bAsync;
    am_hal_xspimc_xfer_mode_t eXferMode;
    uint32_t                  ui32NumPacket;
    uint32_t                  ui32PacketsDone;
    uint32_t                  ui32PacketsLeft;
} am_hal_xspimc_xfer_ctx_status_t;

typedef struct
{
    uint32_t *pDMACmdList;
    uint32_t ui32DMACmdListSize;
    uint8_t  ui8DMABurstLen;
    uint8_t  ui8FIFOLevel;
    bool     bDMAIRQALO;
} am_hal_xspimc_cfg_t;

typedef struct
{
    union
    {
        uint8_t ui8GpioNum;
        uint8_t ui8HWCENum;
    };
    am_hal_xspimc_ce_mode_e eCEMode;
} am_hal_xspimc_ce_t;

typedef struct
{
    //
    //! @brief Maximum CE active time (nanoseconds).
    //! If non-zero, this can be used to automatically break long CE assertions.
    //
    uint32_t ui32TCEMaxAct;                  //max cs low
    //
    //! @brief The minimum CE active time before the first CLK and after the last CLK active edge.
    //! Specified in nanoseconds.
    //
    uint32_t ui32TCESetupHold;               //min cs low
    //
    //! @brief The minimum CE inactive time between transfers.
    //! Specified in nanoseconds.
    //
    uint32_t ui32TCEMinInact;                //min CS High
} am_hal_xspimc_ce_timing;

typedef am_hal_xspimc_ce_t am_hal_xspimc_dev_id;

typedef struct
{
    uint32_t                  ui32Freq;
    am_hal_xspimc_io_mode_e   eIOMode;
    am_hal_xspimc_data_rate_e eDataRate;
    am_hal_xspimc_cpp_mode_e  eCpp;
    bool                      bDQS;
    uint32_t                  ui32ReadCmd;
    uint32_t                  ui32WriteCmd;
    uint32_t                  ui32Mode;
    uint8_t                   ui8CmdLen;
    uint8_t                   ui8AddrLen;
    uint8_t                   ui8AddrAlign;
    uint8_t                   ui8ModeLen;
    uint8_t                   ui8RxDummy;
    uint8_t                   ui8TxDummy;
    uint8_t                   ui8DynTxDummy;
    uint32_t                  ui32MemBoundary;
    am_hal_xspimc_ce_timing   sCETiming;
    am_hal_xspimc_dev_type_e  eDeviceType;
} am_hal_xspimc_dev_cfg_t;

typedef struct
{
    uint32_t                   ui32AIPBaseAddr;
    uint32_t                   ui32MappedDevBaseOffset;
    uint32_t                   ui32AIPSize;
    bool                       bAIPEnable;
    uint32_t                   ui32PageSize;
    uint8_t                    ui8WindowNum;
    am_hal_xspimc_aip_permit_e ePermission;        // Write support or not
    bool                       bPrefetch;
} am_hal_xspimc_aip_cfg_t;

//*****************************************************************************
//
//! @brief Maximum number of write/erase command descriptors cached per slot.
//!
//! @ingroup xspimc_at110
//
//*****************************************************************************
#define AM_HAL_XSPIMC_RWW_MAX_WRITE_ERASE_CMDS 5u

//*****************************************************************************
//
//! @brief Resolved raw write/erase command descriptor cached inside the HAL.
//!
//! @ingroup xspimc_at110
//!
//! Stores the opcode and command length of one program/erase operation the
//! high-level transfer path must recognize for read-while-write. The HAL owns
//! a stable copy so it never depends on flash-layer enums after configuration.
//
//*****************************************************************************
typedef struct
{
    uint16_t ui16Cmd;
    uint8_t  ui8CmdLen;
} am_hal_xspimc_rww_cmd_t;

typedef struct
{
    //! @brief Suspend command opcode.
    uint16_t                  ui16SuspendCmd;
    //! @brief Resume command opcode.
    uint16_t                  ui16ResumeCmd;

    //! @brief Suspend command execution time, specified in microseconds.
    uint32_t                  ui32SuspendTimeUs;
    //! @brief Resume command execution time, specified in microseconds.
    uint32_t                  ui32ResumeTimeUs;
    //! @brief Minimum time from suspend completion to resume command issue, in microseconds.
    uint32_t                  ui32SuspendToResumeTimeUs;
    //! @brief Minimum time from resume completion to suspend command issue, in microseconds.
    uint32_t                  ui32ResumeToSuspendTimeUs;
    //! @brief Read-while-write holdoff guard time, in microseconds.
    uint32_t                  ui32HoldoffTimeUs;

    //! @brief Resolved active-protocol write/erase command descriptors.
    am_hal_xspimc_rww_cmd_t   asWriteEraseCmds[AM_HAL_XSPIMC_RWW_MAX_WRITE_ERASE_CMDS];
    //! @brief Number of valid entries in @ref asWriteEraseCmds.
    uint8_t                   ui8NumWriteEraseCmds;

    //! @brief IO mode used for the suspend/resume command phase.
    am_hal_xspimc_io_mode_e   eIOMode;
    //! @brief Data-rate (STR/DDR) used for the suspend/resume command phase.
    am_hal_xspimc_data_rate_e eDataRate;
    //! @brief Suspend/resume command length in bytes (used to derive SRCMDLEN encoding).
    uint8_t                   ui8CmdLen;
    //! @brief Enable automatic suspend/resume handling for this window.
    bool                      bAutoSusResEnable;
} am_hal_xspimc_sus_res_cfg_t;

typedef struct
{
    int8_t i8SampleOffset;
    uint8_t ui8DQSDelay;
    uint8_t ui8DataDelay;
} am_hal_xspimc_timing_cfg_t;

//*****************************************************************************
//
//! @brief Configure for base input delay for custom SPI and AIP.
//!
//! All delay values are hardware tap encodings.
//
//*****************************************************************************
typedef struct
{
    //! Input delay applied to DQS signals (0..IMPDELAY, at most 255 each).
    uint8_t ui8DQS0Delay;
    uint8_t ui8DQS1Delay;
    //! Input delays for DQ0 through DQ15 (0..IMPDELAY, at most 255 each).
    uint8_t ui8Data0Delay;
    uint8_t ui8Data1Delay;
    uint8_t ui8Data2Delay;
    uint8_t ui8Data3Delay;
    uint8_t ui8Data4Delay;
    uint8_t ui8Data5Delay;
    uint8_t ui8Data6Delay;
    uint8_t ui8Data7Delay;
    uint8_t ui8Data8Delay;
    uint8_t ui8Data9Delay;
    uint8_t ui8Data10Delay;
    uint8_t ui8Data11Delay;
    uint8_t ui8Data12Delay;
    uint8_t ui8Data13Delay;
    uint8_t ui8Data14Delay;
    uint8_t ui8Data15Delay;
} am_hal_xspimc_delay_cfg_t;

typedef struct
{
    bool     bDrive;
    bool     bDriveHigh;
} am_hal_xspimc_wp_hold_cfg_t;

typedef struct
{
    //! @brief  Direction (Transmit/Receive)
    am_hal_xspimc_xfer_dir_e eDir;
    //! @brief  Bus event callbacks to trigger
    uint8_t                  ui8CBMask;
    //! @brief  Transfer command
    uint16_t                 ui16Cmd;
    //! @brief  Transfer Address
    uint32_t                 ui32Address;
    //! @brief  Number of bytes to transfer
    uint32_t                 ui32NumBytes;
    //! @brief  Data Buffer
    uint8_t                 *pDataBuff;
} am_hal_xspimc_xfer_packet_t;

typedef struct
{
    //! @brief  Async or sync transfer
    bool                              bAsync;
    //! @brief  Transfer Mode
    am_hal_xspimc_xfer_mode_t         eXferMode;
    //! @brief  Configure TX dummy cycles
    uint8_t                           ui8TxDummy;
    //! @brief  Configure RX dummy cycles
    uint8_t                           ui8RxDummy;
    //! @brief  Configure mode bits (value)
    uint32_t                          ui32ModeBits;
    //! @brief  Configure mode bits length (same encoding as SPIMODEBITSATTR.LEN)
    uint8_t                           ui8ModeBitLen;
    //! @brief  Configure command length
    uint8_t                           ui8CmdLen;
    //! @brief  Configure address length
    uint8_t                           ui8AddrLen;
    //! @brief  Transfer packets
    const am_hal_xspimc_xfer_packet_t *pPackets;
    //! @brief  Number of transfer packets
    uint32_t                          ui32NumPacket;
    //! @brief  Transfer timeout (microseconds). For DMA synchronous transfers,
    //! a value of 0 uses an estimated default based on the transfer size.
    uint32_t                          ui32Timeout;
} am_hal_xspimc_xfer_t;

typedef struct
{
    //! @brief  Transfer command
    uint16_t                 ui16Cmd;
    //! @brief  Transfer command length
    uint8_t                  ui8CmdLen;
    //! @brief  Transfer Address length
    uint8_t                  ui8AddrLen;
    //! @brief  Transfer Address
    uint32_t                 ui32Address;
    //! @brief  Number of bytes to transfer
    uint32_t                 ui32NumBytes;
    //! @brief  Data Buffer
    uint8_t                 *pDataBuff;
} am_hal_xspimc_lp_cmdinfo_t;

typedef struct
{
    //! @brief Low-power entry/exit enable
    bool                       bEnable;
    //! @brief Low-power cmd frequency
    uint32_t                   ui32Freq;
    //! @brief Low-power cmd IO mode
    am_hal_xspimc_io_mode_e    eIOMode;
    //! @brief Low-power cmd data rate
    am_hal_xspimc_data_rate_e  eDataRate;
    //! @brief Low-power entry cmd info
    am_hal_xspimc_lp_cmdinfo_t sCmdEntry;
    //! @brief Low-power exit cmd info
    am_hal_xspimc_lp_cmdinfo_t sCmdExit;
    //! @brief Minimum time to stay in low power state after entry,
    //! specified in microseconds.
    uint32_t                   ui32MinEntryTime;
    //! @brief Minimum time to stay in normal state after exit,
    //! specified in microseconds.
    uint32_t                   ui32MinExitTime;
} am_hal_xspimc_lp_cfg_t;

typedef struct
{
    //! @brief Pointer to the bus controller
    void                              *pHandle;
    //! @brief Pointer to the peripheral device ID
    const am_hal_xspimc_dev_id        *pDevice;
    //! @brief Pointer to a transfer packet
    const am_hal_xspimc_xfer_packet_t *pPacket;
    //! @brief Packet index
    uint32_t                           ui32PacketIdx;
} am_hal_xspimc_event_data_t;

typedef struct
{
    //! Event type
    am_hal_xspimc_event_e      eEvent;
    //! Data associated to the event
    am_hal_xspimc_event_data_t sData;
} am_hal_xspimc_event_t;

typedef struct
{
    //! @brief xSPI event
    am_hal_xspimc_event_t sEvent;
    //! @brief user defined context
    void                 *pUserCtx;
} am_hal_xspimc_cb_ctx_t;

typedef void (*am_hal_xspimc_callback_t)(am_hal_xspimc_cb_ctx_t *pCbCtx);

typedef struct
{
    am_hal_xspimc_callback_type_e eCBType;
    am_hal_xspimc_callback_t      userCallback;
    void                         *userContext;
} am_hal_xspimc_callback_cfg_t;

typedef struct
{
    uint32_t (*enable)              (am_hal_periph_handle_t *pHandle,
                                     const am_hal_xspimc_dev_id *pDevice);
    uint32_t (*disable)             (am_hal_periph_handle_t *pHandle,
                                     const am_hal_xspimc_dev_id *pDevice);
    uint32_t (*power_control)       (am_hal_periph_handle_t *pHandle,
                                     const am_hal_xspimc_dev_id *pDevice,
                                     am_hal_xspimc_power_req_e ePowerReq);
    uint32_t (*configure)           (am_hal_periph_handle_t *pHandle,
                                     const am_hal_xspimc_dev_id *pDevice,
                                     const am_hal_xspimc_cfg_t *pConfig);
    uint32_t (*device_configure)    (am_hal_periph_handle_t *pHandle,
                                     const am_hal_xspimc_dev_id *pDevice,
                                     const am_hal_xspimc_dev_cfg_t *pConfig);
    uint32_t (*control)             (am_hal_periph_handle_t *pHandle,
                                     const am_hal_xspimc_dev_id *pDevice,
                                     am_hal_xspimc_control_e eControl,
                                     const void *pConfig);
    uint32_t (*xfer)                (am_hal_periph_handle_t *pHandle,
                                     const am_hal_xspimc_dev_id *pDevice,
                                     const am_hal_xspimc_xfer_t *pXfer);
    uint32_t (*status)              (am_hal_periph_handle_t *pHandle,
                                     am_hal_xspimc_status_type_e eType,
                                     void *pStatus);
    uint32_t (*capability)          (am_hal_periph_handle_t *pHandle,
                                     am_hal_xspimc_capability_t *pXSPICaps);
} am_hal_xspimc_api_t;


//! @} END_GROUP - xSPI public types

//*****************************************************************************
//
//! @brief xSPI configuration helper functions
//! @defgroup xspimc_helpers_at110
//! @ingroup xspimc_at110
//! @{
//!
//! These inline helpers convert a high-level `am_hal_xspimc_dev_cfg_t` into the
//! corresponding low-level xSPI register attribute unions used by the LL
//! configuration APIs (e.g. `am_hal_xspimc_ll_cmd_phase_config()`).
//!
//! Rationale:
//! - Keeps the conversion logic in one place (HAL + tests can share it)
//! - Avoids requiring testcases to duplicate or re-implement internal mappings
//!
//*****************************************************************************

// Helper forward declarations (must appear before first use in this header).
static inline uint32_t
xspimc_lines(uint8_t ui8IOMode)
{
    switch (ui8IOMode)
    {
        case 0x2:  return 2u;
        case 0x4:  return 4u;
        case 0x8:  return 8u;
        case 0x9:  return 16u;
        default:   return 1u;
    }
}

static inline bool
xspimc_dtr_cmd(am_hal_xspimc_data_rate_e eRate)
{
    return (eRate == AM_HAL_XSPIMC_DATA_RATE_DUAL);
}

static inline bool
xspimc_dtr_addr(am_hal_xspimc_data_rate_e eRate)
{
    return (eRate >= AM_HAL_XSPIMC_DATA_RATE_S_D_D);
}

static inline bool
xspimc_dtr_mode(am_hal_xspimc_data_rate_e eRate)
{
    return (eRate >= AM_HAL_XSPIMC_DATA_RATE_S_D_D);
}

static inline bool
xspimc_dtr_data(am_hal_xspimc_data_rate_e eRate)
{
    return (eRate >= AM_HAL_XSPIMC_DATA_RATE_S_S_D);
}

// LEN fields encode (SPI clock cycles - 1) and must match the phase length exactly
// (no ceiling). This helper enforces divisibility and field range.
static inline uint32_t
xspimc_len_from_bits(uint64_t ui64Bits,
                     uint32_t ui32Lines,
                     bool bDTR,
                     uint32_t ui32LenMax,
                     uint32_t *pui32Len)
{
    uint32_t ui32BitsPerCycle = ui32Lines * (bDTR ? 2u : 1u);
    if ((ui64Bits % (uint64_t)ui32BitsPerCycle) != 0ull)
    {
        return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    uint64_t ui64Cycles = ui64Bits / (uint64_t)ui32BitsPerCycle;
    if (ui64Cycles == 0ull)
    {
        return AM_HAL_STATUS_OUT_OF_RANGE;
    }
    uint64_t ui64Len = ui64Cycles - 1ull;
    if (ui64Len > (uint64_t)ui32LenMax)
    {
        return AM_HAL_STATUS_OUT_OF_RANGE;
    }
    *pui32Len = (uint32_t)ui64Len;
    return AM_HAL_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Pack up to 4 bytes into a 32-bit word (destination may be unaligned).
//!
//! @param pui32Dst - destination word (accumulated with OR).
//! @param pui8Src  - source byte buffer.
//! @param ui32Len  - number of bytes to pack (1..4).
//!
//! @return Number of bytes packed.
//
//*****************************************************************************
static inline void
am_hal_xspimc_cov_endian_32(uint32_t *pui32Dst, const uint8_t *pui8Src, uint32_t ui32Len)
{
    *pui32Dst = 0;
    switch (ui32Len)
    {
        case 4:
            *pui32Dst = *(const uint32_t *)pui8Src;
            break;
        case 3:
            *pui32Dst |= ((uint32_t)pui8Src[2] << 24);
            *pui32Dst |= ((uint32_t)pui8Src[1] << 16);
            *pui32Dst |= ((uint32_t)pui8Src[0] << 8);
            break;
        case 2:
            *pui32Dst |= ((uint32_t)pui8Src[1] << 24);
            *pui32Dst |= ((uint32_t)pui8Src[0] << 16);
            break;
        case 1:
            *pui32Dst |= ((uint32_t)pui8Src[0] << 24);
            break;
        default:
            break;
    }
}

//*****************************************************************************
//
//! @brief Pack up to 4 bytes into a 32-bit word (destination may be unaligned).
//!
//! @param pu16Dst - destination word (accumulated with OR).
//! @param pui8Src  - source byte buffer.
//! @param ui32Len  - number of bytes to pack (1..2).
//!
//! @return Number of bytes packed.
//
//*****************************************************************************
static inline void
am_hal_xspimc_cov_endian_16(uint16_t *pu16Dst, const uint8_t *pui8Src, uint32_t ui32Len)
{
    *pu16Dst = 0;
    switch (ui32Len)
    {
        case 2:
            *pu16Dst = *(const uint16_t *)pui8Src;
            break;
        case 1:
            *pu16Dst |= ((uint16_t)pui8Src[0] << 8);
            __fallthrough;
        default:
            break;
    }
}

static inline uint32_t
am_hal_xspimc_cov_dev_freq(const am_hal_xspimc_dev_cfg_t *pCfg,
                           uint32_t ui32Source,
                           uint32_t *pui32ClkDiv)
{
    //
    // Compute CLOCKPERIOD
    // We use ceiling division so the actual SCK never exceeds the requested frequency.
    //
    if (pCfg->ui32Freq == 0)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    {
        *pui32ClkDiv = (ui32Source + (pCfg->ui32Freq / 2u)) / pCfg->ui32Freq;
        if (*pui32ClkDiv > 255u || *pui32ClkDiv <= 1)
        {
            return AM_HAL_STATUS_OUT_OF_RANGE;
        }
    }
    return AM_HAL_STATUS_SUCCESS;
}

static inline uint32_t
am_hal_xspimc_cov_dev_cmd_attr(const am_hal_xspimc_dev_cfg_t *pCfg,
                               am_hal_xspimc_ll_cmd_attr_t *pAttr)
{
    pAttr->xSPICmdAttr              = 0;
    pAttr->xSPICmdAttr_b.xSPICmdEN  = (pCfg->ui8CmdLen != 0u) ? 0u : 1u;
    pAttr->xSPICmdAttr_b.xSPICmdDTR = xspimc_dtr_cmd(pCfg->eDataRate) ? 1u : 0u;
    switch (pCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_2_2:
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_4_4:
        case AM_HAL_XSPIMC_IO_1_1_8:
        case AM_HAL_XSPIMC_IO_1_8_8:
            pAttr->xSPICmdAttr_b.xSPICmdMode = AM_HAL_XSPIMC_LL_IO_MODE_SINGLE;
            break;
        case AM_HAL_XSPIMC_IO_DUAL:
            pAttr->xSPICmdAttr_b.xSPICmdMode = AM_HAL_XSPIMC_LL_IO_MODE_DUAL;
            break;
        case AM_HAL_XSPIMC_IO_QUAD:
            pAttr->xSPICmdAttr_b.xSPICmdMode = AM_HAL_XSPIMC_LL_IO_MODE_QUAD;
            break;
        case AM_HAL_XSPIMC_IO_OCTAL:
        case AM_HAL_XSPIMC_IO_8_8_16:
        case AM_HAL_XSPIMC_IO_8_16_16:
            pAttr->xSPICmdAttr_b.xSPICmdMode = AM_HAL_XSPIMC_LL_IO_MODE_OCTA;
            break;
        case AM_HAL_XSPIMC_IO_HEX:
            pAttr->xSPICmdAttr_b.xSPICmdMode = AM_HAL_XSPIMC_LL_IO_MODE_HEX;
            break;
        default:
            return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    if (pCfg->ui8CmdLen == 0u)
    {
        pAttr->xSPICmdAttr_b.xSPICmdLen = 0u;
        return AM_HAL_STATUS_SUCCESS;
    }

    uint32_t ui32Len    = 0u;
    uint32_t ui32Status = xspimc_len_from_bits((uint64_t)pCfg->ui8CmdLen * 8ull,
                                               xspimc_lines(pAttr->xSPICmdAttr_b.xSPICmdMode),
                                               pAttr->xSPICmdAttr_b.xSPICmdDTR,
                                               0xFu, &ui32Len);
    if (ui32Status)
    {
        return ui32Status;
    }

    pAttr->xSPICmdAttr_b.xSPICmdLen = ui32Len;

    if (pCfg->eDeviceType == AM_HAL_XSPIMC_HYPERRAM_PSRAM_DEVICE)
    {
        pAttr->xSPICmdAttr_b.xSPICmdHRC = 1u;
    }

    return AM_HAL_STATUS_SUCCESS;
}

static inline uint32_t
am_hal_xspimc_cov_dev_addr_attr(const am_hal_xspimc_dev_cfg_t *pCfg,
                                am_hal_xspimc_ll_addr_attr_t *pAttr)
{
    pAttr->xSPIAddrAttr               = 0;
    pAttr->xSPIAddrAttr_b.xSPIAddrEN  = (pCfg->ui8AddrLen != 0u) ? 0u : 1u;
    pAttr->xSPIAddrAttr_b.xSPIAddrDTR = xspimc_dtr_addr(pCfg->eDataRate) ? 1u : 0u;
    switch (pCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_1_8:
            pAttr->xSPIAddrAttr_b.xSPIAddrMode = AM_HAL_XSPIMC_LL_IO_MODE_SINGLE;
            break;
        case AM_HAL_XSPIMC_IO_DUAL:
        case AM_HAL_XSPIMC_IO_1_2_2:
            pAttr->xSPIAddrAttr_b.xSPIAddrMode = AM_HAL_XSPIMC_LL_IO_MODE_DUAL;
            break;
        case AM_HAL_XSPIMC_IO_QUAD:
        case AM_HAL_XSPIMC_IO_1_4_4:
            pAttr->xSPIAddrAttr_b.xSPIAddrMode = AM_HAL_XSPIMC_LL_IO_MODE_QUAD;
            break;
        case AM_HAL_XSPIMC_IO_OCTAL:
        case AM_HAL_XSPIMC_IO_1_8_8:
        case AM_HAL_XSPIMC_IO_8_8_16:
            pAttr->xSPIAddrAttr_b.xSPIAddrMode = AM_HAL_XSPIMC_LL_IO_MODE_OCTA;
            break;
        case AM_HAL_XSPIMC_IO_HEX:
        case AM_HAL_XSPIMC_IO_8_16_16:
            pAttr->xSPIAddrAttr_b.xSPIAddrMode = AM_HAL_XSPIMC_LL_IO_MODE_HEX;
            break;
        default:
            return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    switch (pCfg->ui8AddrAlign)
    {
        case 1:
            pAttr->xSPIAddrAttr_b.xSPIAddrMask = 0x7;
            break;
        case 2:
            pAttr->xSPIAddrAttr_b.xSPIAddrMask = 0x6;
            break;
        case 4:
            pAttr->xSPIAddrAttr_b.xSPIAddrMask = 0x4;
            break;
        case 8:
            pAttr->xSPIAddrAttr_b.xSPIAddrMask = 0x0;
            break;
        default:
            return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    if (pCfg->ui8AddrLen == 0u)
    {
        pAttr->xSPIAddrAttr_b.xSPIAddrLen = 0u;
        return AM_HAL_STATUS_SUCCESS;
    }

    uint32_t ui32Len    = 0u;
    uint32_t ui32Status = xspimc_len_from_bits((uint64_t)pCfg->ui8AddrLen * 8ull,
                                               xspimc_lines(pAttr->xSPIAddrAttr_b.xSPIAddrMode),
                                               pAttr->xSPIAddrAttr_b.xSPIAddrDTR,
                                               0x1Fu, &ui32Len);
    if (ui32Status)
    {
        return ui32Status;
    }

    if (pCfg->eDeviceType == AM_HAL_XSPIMC_APM_HEX_PSRAM_DEVICE)
    {
        pAttr->xSPIAddrAttr_b.xSPIAddrTrans = 1;
        pAttr->xSPIAddrAttr_b.xSPIAddrShift = 1;
    }
    else if ( pCfg->eDeviceType == AM_HAL_XSPIMC_HYPERRAM_PSRAM_DEVICE &&
              pCfg->eIOMode == AM_HAL_XSPIMC_IO_8_8_16 )
    {
        pAttr->xSPIAddrAttr_b.xSPIAddrShift = 2;
    }

    pAttr->xSPIAddrAttr_b.xSPIAddrLen = ui32Len;

    return AM_HAL_STATUS_SUCCESS;
}

static inline uint32_t
am_hal_xspimc_cov_dev_modebit_attr(const am_hal_xspimc_dev_cfg_t *pCfg,
                                   am_hal_xspimc_ll_modebit_attr_t *pAttr)
{
    pAttr->xSPIModeBitAttr                  = 0;
    pAttr->xSPIModeBitAttr_b.xSPIModeBitEN  = (pCfg->ui8ModeLen != 0u) ? 0u : 1u;
    pAttr->xSPIModeBitAttr_b.xSPIModeBitDTR = xspimc_dtr_mode(pCfg->eDataRate) ? 1u : 0u;
    // Mode bit follows ADDR phase IO Mode.
    switch (pCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_1_8:
            pAttr->xSPIModeBitAttr_b.xSPIModeBitMode = AM_HAL_XSPIMC_LL_IO_MODE_SINGLE;
            break;
        case AM_HAL_XSPIMC_IO_DUAL:
        case AM_HAL_XSPIMC_IO_1_2_2:
            pAttr->xSPIModeBitAttr_b.xSPIModeBitMode = AM_HAL_XSPIMC_LL_IO_MODE_DUAL;
            break;
        case AM_HAL_XSPIMC_IO_QUAD:
        case AM_HAL_XSPIMC_IO_1_4_4:
            pAttr->xSPIModeBitAttr_b.xSPIModeBitMode = AM_HAL_XSPIMC_LL_IO_MODE_QUAD;
            break;
        case AM_HAL_XSPIMC_IO_OCTAL:
        case AM_HAL_XSPIMC_IO_1_8_8:
        case AM_HAL_XSPIMC_IO_8_8_16:
            pAttr->xSPIModeBitAttr_b.xSPIModeBitMode = AM_HAL_XSPIMC_LL_IO_MODE_OCTA;
            break;
        case AM_HAL_XSPIMC_IO_HEX:
        case AM_HAL_XSPIMC_IO_8_16_16:
            pAttr->xSPIModeBitAttr_b.xSPIModeBitMode = AM_HAL_XSPIMC_LL_IO_MODE_HEX;
            break;
        default:
            return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    if (pCfg->ui8ModeLen == 0u)
    {
        pAttr->xSPIModeBitAttr_b.xSPIModeBitLen = 0u;
        return AM_HAL_STATUS_SUCCESS;
    }

    uint32_t ui32Len    = 0u;
    uint32_t ui32Status = xspimc_len_from_bits((uint64_t)pCfg->ui8ModeLen * 8ull,
                                               xspimc_lines(
                                                   pAttr->xSPIModeBitAttr_b.xSPIModeBitMode),
                                               pAttr->xSPIModeBitAttr_b.xSPIModeBitDTR,
                                               0x1Fu,
                                               &ui32Len);
    if (ui32Status)
    {
        return ui32Status;
    }

    pAttr->xSPIModeBitAttr_b.xSPIModeBitLen = ui32Len;
    return AM_HAL_STATUS_SUCCESS;
}

static inline uint32_t
am_hal_xspimc_cov_xfer_cmd_attr(XSPIMC_Type *pReg,
                                const am_hal_xspimc_xfer_t *pXfer,
                                am_hal_xspimc_ll_cmd_attr_t *pAttr)
{
    // Read current hardware policy (width + STR/DDR) from SPICMDATTR.
    pAttr->xSPICmdAttr = am_hal_xspimc_ll_cmd_attr_get(pReg);

    // xfer always overrides, including 0 meaning "disable".
    pAttr->xSPICmdAttr_b.xSPICmdEN = (pXfer->ui8CmdLen != 0u) ? 0u : 1u;

    if (pXfer->ui8CmdLen == 0u)
    {
        pAttr->xSPICmdAttr_b.xSPICmdLen = 0u;
        return AM_HAL_STATUS_SUCCESS;
    }

    uint32_t ui32Len    = 0u;
    uint32_t ui32Status = xspimc_len_from_bits((uint64_t)pXfer->ui8CmdLen * 8ull,
                                               xspimc_lines(pAttr->xSPICmdAttr_b.xSPICmdMode),
                                               pAttr->xSPICmdAttr_b.xSPICmdDTR,
                                               0xFu, &ui32Len);
    if (ui32Status)
    {
        return ui32Status;
    }

    pAttr->xSPICmdAttr_b.xSPICmdLen = ui32Len;
    return AM_HAL_STATUS_SUCCESS;
}

static inline uint32_t
am_hal_xspimc_cov_xfer_addr_attr(XSPIMC_Type *pReg,
                                 const am_hal_xspimc_xfer_t *pXfer,
                                 am_hal_xspimc_ll_addr_attr_t *pAttr)
{
    // Read current hardware policy (width + STR/DDR) from SPIADDRATTR.
    pAttr->xSPIAddrAttr = am_hal_xspimc_ll_addr_attr_get(pReg);

    // xfer always overrides, including 0 meaning "disable".
    pAttr->xSPIAddrAttr_b.xSPIAddrEN = (pXfer->ui8AddrLen != 0u) ? 0u : 1u;

    if (pXfer->ui8AddrLen == 0u)
    {
        pAttr->xSPIAddrAttr_b.xSPIAddrLen = 0u;
        return AM_HAL_STATUS_SUCCESS;
    }

    uint32_t ui32Len    = 0u;
    uint32_t ui32Status = xspimc_len_from_bits((uint64_t)pXfer->ui8AddrLen * 8ull,
                                               xspimc_lines(pAttr->xSPIAddrAttr_b.xSPIAddrMode),
                                               pAttr->xSPIAddrAttr_b.xSPIAddrDTR,
                                               0x1Fu, &ui32Len);
    if (ui32Status)
    {
        return ui32Status;
    }
    pAttr->xSPIAddrAttr_b.xSPIAddrLen = ui32Len;

    return AM_HAL_STATUS_SUCCESS;
}

static inline uint32_t
am_hal_xspimc_cov_xfer_modebit_attr(XSPIMC_Type *pReg,
                                    const am_hal_xspimc_xfer_t *pXfer,
                                    am_hal_xspimc_ll_modebit_attr_t *pAttr)
{
    // Read current hardware policy (width + STR/DDR) from SPIMODEBITSATTR.
    pAttr->xSPIModeBitAttr = am_hal_xspimc_ll_modebits_attr_get(pReg);

    // xfer always overrides, including 0 meaning "disable".
    pAttr->xSPIModeBitAttr_b.xSPIModeBitEN = (pXfer->ui8ModeBitLen != 0u) ? 0u : 1u; // skip bit

    if (pXfer->ui8ModeBitLen == 0u)
    {
        pAttr->xSPIModeBitAttr_b.xSPIModeBitLen = 0u;
        return AM_HAL_STATUS_SUCCESS;
    }

    uint32_t ui32Len    = 0u;
    uint32_t ui32Status = xspimc_len_from_bits((uint64_t)pXfer->ui8ModeBitLen * 8ull,
                                               xspimc_lines(
                                                   pAttr->xSPIModeBitAttr_b.xSPIModeBitMode),
                                               pAttr->xSPIModeBitAttr_b.xSPIModeBitDTR,
                                               0x1Fu,
                                               &ui32Len);
    if (ui32Status)
    {
        return ui32Status;
    }

    pAttr->xSPIModeBitAttr_b.xSPIModeBitLen = ui32Len;

    return AM_HAL_STATUS_SUCCESS;
}

static inline void
am_hal_xspimc_cov_xfer_dummy_attr(XSPIMC_Type *pReg,
                                  const am_hal_xspimc_xfer_t *pXfer,
                                  am_hal_xspimc_xfer_dir_e eDir,
                                  am_hal_xspimc_ll_dummy_attr_t *pAttr)
{
    pAttr->xSPIDummyAttr = am_hal_xspimc_ll_dummy_attr_get(pReg);

    if (eDir == AM_HAL_XSPIMC_XFER_WRITE)
    {
        pAttr->xSPIDummyAttr_b.xSPIWLCEN = pXfer->ui8TxDummy ? 0 : 1;
        pAttr->xSPIDummyAttr_b.xSPIWLC   = pXfer->ui8TxDummy ? pXfer->ui8TxDummy - 1 : 0;
    }
    else
    {
        pAttr->xSPIDummyAttr_b.xSPIRLCEN = pXfer->ui8RxDummy ? 0 : 1;
        pAttr->xSPIDummyAttr_b.xSPIRLC   = pXfer->ui8RxDummy ? pXfer->ui8RxDummy -1 : 0;
    }
}

static inline void
am_hal_xspimc_cov_dev_dummy_attr(const am_hal_xspimc_dev_cfg_t *pCfg,
                                 am_hal_xspimc_ll_dummy_attr_t *pAttr)
{
    pAttr->xSPIDummyAttr = 0;

    pAttr->xSPIDummyAttr_b.xSPIWLCEN    = pCfg->ui8TxDummy ? 0 : 1;
    pAttr->xSPIDummyAttr_b.xSPIWLC      = pCfg->ui8TxDummy ? pCfg->ui8TxDummy - 1 : 0;
    pAttr->xSPIDummyAttr_b.xSPIRLCEN    = pCfg->ui8RxDummy ? 0 : 1;
    pAttr->xSPIDummyAttr_b.xSPIRLC      = pCfg->ui8RxDummy ? pCfg->ui8RxDummy -1 : 0;
    pAttr->xSPIDummyAttr_b.xSPIDYNWLCEN = pCfg->ui8DynTxDummy ? 1 : 0;
    pAttr->xSPIDummyAttr_b.xSPIDYNWLC   = pCfg->ui8DynTxDummy ? pCfg->ui8DynTxDummy - 1 : 0;
}

static inline void
am_hal_xspimc_cov_dev_data_attr(const am_hal_xspimc_dev_cfg_t *pCfg,
                                am_hal_xspimc_ll_data_attr_t *pAttr)
{
    pAttr->xSPIDataAttr = 0;

    pAttr->xSPIDataAttr_b.xSPIDataDQS = pCfg->bDQS;
    pAttr->xSPIDataAttr_b.xSPIDataDTR = xspimc_dtr_data(pCfg->eDataRate) ? 1u : 0u;
    switch (pCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
            pAttr->xSPIDataAttr_b.xSPIDataMode = AM_HAL_XSPIMC_LL_IO_MODE_SINGLE;
            break;
        case AM_HAL_XSPIMC_IO_DUAL:
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_2_2:
            pAttr->xSPIDataAttr_b.xSPIDataMode = AM_HAL_XSPIMC_LL_IO_MODE_DUAL;
            break;
        case AM_HAL_XSPIMC_IO_QUAD:
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_4_4:
            pAttr->xSPIDataAttr_b.xSPIDataMode = AM_HAL_XSPIMC_LL_IO_MODE_QUAD;
            break;
        case AM_HAL_XSPIMC_IO_OCTAL:
        case AM_HAL_XSPIMC_IO_1_1_8:
        case AM_HAL_XSPIMC_IO_1_8_8:
            pAttr->xSPIDataAttr_b.xSPIDataMode = AM_HAL_XSPIMC_LL_IO_MODE_OCTA;
            break;
        case AM_HAL_XSPIMC_IO_HEX:
        case AM_HAL_XSPIMC_IO_8_16_16:
        case AM_HAL_XSPIMC_IO_8_8_16:
            pAttr->xSPIDataAttr_b.xSPIDataMode = AM_HAL_XSPIMC_LL_IO_MODE_HEX;
            break;
        default:
            break;
    }
}

static inline void
am_hal_xspimc_cov_dev_aip_cmd(const am_hal_xspimc_dev_cfg_t *pCfg, am_hal_xspimc_ll_aip_cmd_t *pCmd)
{
    uint16_t ui16RCMD = 0;
    uint16_t ui16WCMD = 0;

    am_hal_xspimc_cov_endian_16(&ui16RCMD, (const uint8_t *)&pCfg->ui32ReadCmd, pCfg->ui8CmdLen);
    am_hal_xspimc_cov_endian_16(&ui16WCMD, (const uint8_t *)&pCfg->ui32WriteCmd, pCfg->ui8CmdLen);

    pCmd->xAIPCMD_b.xAIPRCMD = ui16RCMD;
    pCmd->xAIPCMD_b.xAIPWCMD = ui16WCMD;
}


static inline uint32_t
am_hal_xspimc_cov_dev_aip_addr_attr(const am_hal_xspimc_dev_cfg_t *pCfg,
                                    am_hal_xspimc_ll_aip_addr_attr_t *pAttr)
{
    pAttr->xAIPADDRATTR = 0;

    pAttr->xAIPADDRATTR_b.xAIPADDRDTR = xspimc_dtr_addr(pCfg->eDataRate) ? 1u : 0u;

    switch (pCfg->ui8AddrAlign)
    {
        case 1:
            pAttr->xAIPADDRATTR_b.xAIPADDRMASK = 0x7;
            break;
        case 2:
            pAttr->xAIPADDRATTR_b.xAIPADDRMASK = 0x6;
            break;
        case 4:
            pAttr->xAIPADDRATTR_b.xAIPADDRMASK = 0x4;
            break;
        case 8:
            pAttr->xAIPADDRATTR_b.xAIPADDRMASK = 0x0;
            break;
        default:
            return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    // Encode address width in 2 bits. Common convention is (bytes - 1):
    // 1B->0, 2B->1, 3B->2, 4B->3.
    if (pCfg->ui8AddrLen)
    {
        switch(pCfg->ui8AddrLen)
        {
            case 1:
                pAttr->xAIPADDRATTR_b.xAIPADDRWIDTH = 3;
                break;
            case 2:
                pAttr->xAIPADDRATTR_b.xAIPADDRWIDTH = 2;
                break;
            case 3:
                pAttr->xAIPADDRATTR_b.xAIPADDRWIDTH = 0;
                break;
            case 4:
                pAttr->xAIPADDRATTR_b.xAIPADDRWIDTH = 1;
                break;
            default:
                return AM_HAL_STATUS_OUT_OF_RANGE;
        }
    }

    switch (pCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_1_8:
            pAttr->xAIPADDRATTR_b.xAIPADDRMODE = AM_HAL_XSPIMC_LL_IO_MODE_SINGLE;
            break;
        case AM_HAL_XSPIMC_IO_DUAL:
        case AM_HAL_XSPIMC_IO_1_2_2:
            pAttr->xAIPADDRATTR_b.xAIPADDRMODE = AM_HAL_XSPIMC_LL_IO_MODE_DUAL;
            break;
        case AM_HAL_XSPIMC_IO_QUAD:
        case AM_HAL_XSPIMC_IO_1_4_4:
            pAttr->xAIPADDRATTR_b.xAIPADDRMODE = AM_HAL_XSPIMC_LL_IO_MODE_QUAD;
            break;
        case AM_HAL_XSPIMC_IO_OCTAL:
        case AM_HAL_XSPIMC_IO_1_8_8:
        case AM_HAL_XSPIMC_IO_8_8_16:
            pAttr->xAIPADDRATTR_b.xAIPADDRMODE = AM_HAL_XSPIMC_LL_IO_MODE_OCTA;
            break;
        case AM_HAL_XSPIMC_IO_HEX:
        case AM_HAL_XSPIMC_IO_8_16_16:
            pAttr->xAIPADDRATTR_b.xAIPADDRMODE = AM_HAL_XSPIMC_LL_IO_MODE_HEX;
            break;
        default:
            return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    if (pCfg->eDeviceType == AM_HAL_XSPIMC_APM_HEX_PSRAM_DEVICE)
    {
        pAttr->xAIPADDRATTR_b.xAIPADDRTRANS = 1;
        pAttr->xAIPADDRATTR_b.xAIPADDRSHIFT = 1;
    }
    else if ( pCfg->eDeviceType == AM_HAL_XSPIMC_HYPERRAM_PSRAM_DEVICE )
    {
        pAttr->xAIPADDRATTR_b.xAIPADDRSHIFT = 2;
    }

    return AM_HAL_STATUS_SUCCESS;
}

static inline void
am_hal_xspimc_cov_dev_aip_data_attr(const am_hal_xspimc_dev_cfg_t *pCfg,
                                    am_hal_xspimc_ll_aip_data_attr_t *pAttr)
{
    pAttr->xAIPDATAATTR = 0;

    pAttr->xAIPDATAATTR_b.xAIPDATADQS = pCfg->bDQS;
    pAttr->xAIPDATAATTR_b.xAIPDATADDR = xspimc_dtr_data(pCfg->eDataRate) ? 1u : 0u;

    switch (pCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
            pAttr->xAIPDATAATTR_b.xAIPDATAMODE = AM_HAL_XSPIMC_LL_IO_MODE_SINGLE;
            break;
        case AM_HAL_XSPIMC_IO_DUAL:
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_2_2:
            pAttr->xAIPDATAATTR_b.xAIPDATAMODE = AM_HAL_XSPIMC_LL_IO_MODE_DUAL;
            break;
        case AM_HAL_XSPIMC_IO_QUAD:
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_4_4:
            pAttr->xAIPDATAATTR_b.xAIPDATAMODE = AM_HAL_XSPIMC_LL_IO_MODE_QUAD;
            break;
        case AM_HAL_XSPIMC_IO_OCTAL:
        case AM_HAL_XSPIMC_IO_1_1_8:
        case AM_HAL_XSPIMC_IO_1_8_8:
            pAttr->xAIPDATAATTR_b.xAIPDATAMODE = AM_HAL_XSPIMC_LL_IO_MODE_OCTA;
            break;
        case AM_HAL_XSPIMC_IO_HEX:
        case AM_HAL_XSPIMC_IO_8_16_16:
        case AM_HAL_XSPIMC_IO_8_8_16:
            pAttr->xAIPDATAATTR_b.xAIPDATAMODE = AM_HAL_XSPIMC_LL_IO_MODE_HEX;
            break;
        default:
            break;
    }
}

static inline void
am_hal_xspimc_cov_dev_ce_timing(const am_hal_xspimc_dev_cfg_t *pCfg,
                                uint32_t  ui32RefFreqHz,
                                uint32_t *pui32MinCELow,
                                uint32_t *pui32MinCEHigh,
                                uint32_t *pui32MaxCELow)
{
    uint64_t ui64Tmp;

    ui64Tmp        = (uint64_t)ui32RefFreqHz * pCfg->sCETiming.ui32TCESetupHold;
    *pui32MinCELow = (uint32_t)((ui64Tmp + 1000000000ULL - 1ULL) / 1000000000ULL);

    ui64Tmp         = (uint64_t)ui32RefFreqHz * pCfg->sCETiming.ui32TCEMinInact;
    *pui32MinCEHigh = (uint32_t)((ui64Tmp + 1000000000ULL - 1ULL) / 1000000000ULL);

    ui64Tmp         = (uint64_t)ui32RefFreqHz * pCfg->sCETiming.ui32TCEMaxAct;
    *pui32MaxCELow  = (uint32_t)((ui64Tmp + 1000000000ULL - 1ULL) / 1000000000ULL);
}

static inline uint32_t
am_hal_xspimc_cov_dev_access_boundary(const am_hal_xspimc_dev_cfg_t *pCfg,
                                      uint32_t ui32CmdCycle,
                                      uint32_t ui32AddrCycle,
                                      uint32_t ui32ModeCycle,
                                      uint32_t ui32DummyCycle,
                                      uint32_t ui32MaxCECycle,
                                      uint32_t *pui32AccessBoundary)
{
    uint32_t ui32DataLines = 0;

    switch (pCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
            ui32DataLines = 1u;
            break;
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_2_2:
        case AM_HAL_XSPIMC_IO_DUAL:
            ui32DataLines = 2u;
            break;
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_4_4:
        case AM_HAL_XSPIMC_IO_QUAD:
            ui32DataLines = 4u;
            break;
        case AM_HAL_XSPIMC_IO_1_1_8:
        case AM_HAL_XSPIMC_IO_1_8_8:
        case AM_HAL_XSPIMC_IO_OCTAL:
            ui32DataLines = 8u;
            break;
        case AM_HAL_XSPIMC_IO_8_8_16:
        case AM_HAL_XSPIMC_IO_8_16_16:
        case AM_HAL_XSPIMC_IO_HEX:
            ui32DataLines = 16u;
            break;
        default:
            return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    uint32_t ui32BitsPerDataCycle = ui32DataLines *
                                    (xspimc_dtr_data(pCfg->eDataRate) ? 2u : 1u);

    uint32_t ui32OverheadCycles = ui32CmdCycle +
                                  ui32AddrCycle +
                                  ui32ModeCycle +
                                  ui32DummyCycle;

    uint32_t ui32DataBytes = 0x80000000u; // effectively no data limit (2GB)
    if (ui32MaxCECycle != 0u)
    {
        if (ui32OverheadCycles >= ui32MaxCECycle)
        {
            return AM_HAL_STATUS_OUT_OF_RANGE;
        }
        else
        {
            ui32DataBytes = (uint32_t)((uint64_t)(ui32MaxCECycle - ui32OverheadCycles) *
                                       (uint64_t)ui32BitsPerDataCycle / 8ull);
        }
    }

    if (pCfg->ui32MemBoundary != 0u)
    {
        *pui32AccessBoundary = pCfg->ui32MemBoundary < ui32DataBytes ?
                               pCfg->ui32MemBoundary : ui32DataBytes;
    }
    else
    {
        *pui32AccessBoundary = ui32DataBytes < 0x80000000u ? ui32DataBytes : 0x80000000u;
    }

    *pui32AccessBoundary = 31 - __builtin_clz(*pui32AccessBoundary);

    return AM_HAL_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Convert suspend/resume configuration into AIP SUS/RES attribute encoding.
//!
//! @param pCfg  - suspend/resume configuration.
//! @param pAttr - output attribute union (AIPSUSRESATTR encoding).
//!
//! @return AM_HAL_STATUS_SUCCESS on success; otherwise a HAL status error code.
//
//*****************************************************************************
static inline uint32_t
am_hal_xspimc_cov_susres_cmd_attr(const am_hal_xspimc_sus_res_cfg_t *pCfg,
                                  am_hal_xspimc_ll_aip_sus_res_attr_t *pAttr)
{
    pAttr->xAIPSUSRESATTR                = 0u;
    pAttr->xAIPSUSRESATTR_b.xAIPSRCmdSD  = 0u;
    pAttr->xAIPSUSRESATTR_b.xAIPSRCmdDTR = xspimc_dtr_cmd(pCfg->eDataRate) ? 1u : 0u;

    switch (pCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_2_2:
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_4_4:
        case AM_HAL_XSPIMC_IO_1_1_8:
        case AM_HAL_XSPIMC_IO_1_8_8:
            pAttr->xAIPSUSRESATTR_b.xAIPSRCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_SINGLE;
            break;
        case AM_HAL_XSPIMC_IO_DUAL:
            pAttr->xAIPSUSRESATTR_b.xAIPSRCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_DUAL;
            break;
        case AM_HAL_XSPIMC_IO_QUAD:
            pAttr->xAIPSUSRESATTR_b.xAIPSRCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_QUAD;
            break;
        case AM_HAL_XSPIMC_IO_OCTAL:
        case AM_HAL_XSPIMC_IO_8_8_16:
        case AM_HAL_XSPIMC_IO_8_16_16:
            pAttr->xAIPSUSRESATTR_b.xAIPSRCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_OCTA;
            break;
        case AM_HAL_XSPIMC_IO_HEX:
            pAttr->xAIPSUSRESATTR_b.xAIPSRCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_HEX;
            break;
        default:
            return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    if (pCfg->ui8CmdLen == 0u)
    {
        pAttr->xAIPSUSRESATTR_b.xAIPSRCMDLEN = 0u;
        return AM_HAL_STATUS_SUCCESS;
    }

    uint32_t ui32Len    = 0u;
    uint32_t ui32Status = xspimc_len_from_bits((uint64_t)pCfg->ui8CmdLen * 8ull,
                                               xspimc_lines(pCfg->eIOMode),
                                               xspimc_dtr_cmd(pCfg->eDataRate),
                                               0xFu, &ui32Len);
    if (ui32Status)
    {
        return ui32Status;
    }

    pAttr->xAIPSUSRESATTR_b.xAIPSRCMDLEN = ui32Len;
    return AM_HAL_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Convert low-power frequency into clock divider.
//!
//! @param pCfg        - low power configuration.
//! @param ui32Source  - source clock frequency in Hz.
//! @param pui32ClkDiv - output clock divider (1-based, i.e. 1 means divide by 1).
//!
//! @return AM_HAL_STATUS_SUCCESS on success; otherwise a HAL status error code.
//*****************************************************************************
static inline uint32_t
am_hal_xspimc_cov_lp_freq(const am_hal_xspimc_lp_cfg_t *pCfg,
                          uint32_t ui32Source,
                          uint32_t *pui32ClkDiv)
{
    //
    // Compute CLOCKPERIOD
    // We use ceiling division so the actual SCK never exceeds the requested frequency.
    //
    if (pCfg->ui32Freq == 0)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    {
        *pui32ClkDiv = (ui32Source + (pCfg->ui32Freq / 2u)) / pCfg->ui32Freq;
        if (*pui32ClkDiv > 255u || *pui32ClkDiv <= 1)
        {
            return AM_HAL_STATUS_OUT_OF_RANGE;
        }
    }
    return AM_HAL_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Convert low-power timing into clock cycles.
//!
//! @param pDevCfg     - device configuration.
//! @param pConfig     - low power configuration.
//! @param ui32BusFreqHz - bus clock frequency in Hz, used for CE timing
//! @param pui32MinCELow - output minimum CE low time in cycles.
//! @param pui32MinEntry - output minimum entry time in cycles.
//! @param pui32MinExit  - output minimum exit time in cycles.
//!
//*****************************************************************************
static inline void
am_hal_xspimc_cov_lp_timing(const am_hal_xspimc_dev_cfg_t *pDevCfg,
                            const am_hal_xspimc_lp_cfg_t *pConfig,
                            uint32_t ui32BusFreqHz,
                            uint32_t *pui32MinCELow,
                            uint32_t *pui32MinEntry,
                            uint32_t *pui32MinExit)
{
    uint64_t ui64Tmp;

    // CE timing is based on bus frequency since CE pin is controlled by SPICLK.
    ui64Tmp        = (uint64_t)ui32BusFreqHz * pDevCfg->sCETiming.ui32TCESetupHold;
    *pui32MinCELow = (uint32_t)((ui64Tmp + 1000000000ULL - 1ULL) / 1000000000ULL);

    // Entry/Exit timing is based on source frequency since it's about how long the device needs to enter/exit low power state after receiving the command.
    ui64Tmp        = (uint64_t)ui32BusFreqHz * pConfig->ui32MinEntryTime;
    *pui32MinEntry = (uint32_t)((ui64Tmp + 1000000ULL - 1ULL) / 1000000ULL);
    ui64Tmp        = (uint64_t)ui32BusFreqHz * pConfig->ui32MinExitTime;
    *pui32MinExit  = (uint32_t)((ui64Tmp + 1000000ULL - 1ULL) / 1000000ULL);
}

//*****************************************************************************
//
//! @brief Convert suspend/resume (read-while-write) timing into bus clock
//!        cycles.
//!
//! @param pCfg              - suspend/resume configuration with microsecond
//!                            timing fields.
//! @param ui32RefHz         - reference bus clock frequency in Hz.
//! @param pui32SuspendTime  - output suspend command time in cycles.
//! @param pui32ResumeTime   - output resume command time in cycles.
//! @param pui32Sus2ResTime  - output suspend-to-resume gap in cycles.
//! @param pui32Res2SusTime  - output resume-to-suspend gap in cycles.
//! @param pui32HoldoffTime  - output read-while-write holdoff time in cycles.
//
//*****************************************************************************
static inline void
am_hal_xspimc_cov_susres_timing(const am_hal_xspimc_sus_res_cfg_t *pCfg,
                                 uint32_t ui32RefHz,
                                 uint32_t *pui32SuspendTime,
                                 uint32_t *pui32ResumeTime,
                                 uint32_t *pui32Sus2ResTime,
                                 uint32_t *pui32Res2SusTime,
                                 uint32_t *pui32HoldoffTime)
{
    uint64_t ui64Tmp;

    ui64Tmp           = (uint64_t)pCfg->ui32SuspendTimeUs * ui32RefHz + 999999ULL;
    *pui32SuspendTime = (uint32_t)(ui64Tmp / 1000000ULL);

    ui64Tmp          = (uint64_t)pCfg->ui32ResumeTimeUs * ui32RefHz + 999999ULL;
    *pui32ResumeTime = (uint32_t)(ui64Tmp / 1000000ULL);

    ui64Tmp           = (uint64_t)pCfg->ui32SuspendToResumeTimeUs * ui32RefHz + 999999ULL;
    *pui32Sus2ResTime = (uint32_t)(ui64Tmp / 1000000ULL);

    ui64Tmp           = (uint64_t)pCfg->ui32ResumeToSuspendTimeUs * ui32RefHz + 999999ULL;
    *pui32Res2SusTime = (uint32_t)(ui64Tmp / 1000000ULL);

    ui64Tmp           = (uint64_t)pCfg->ui32HoldoffTimeUs * ui32RefHz + 999999ULL;
    *pui32HoldoffTime = (uint32_t)(ui64Tmp / 1000000ULL);
}

//*****************************************************************************
//
//! @brief Convert low-power command information into register command words.
//!
//! @param pCmdInfo - low power command information
//! @param pCmd1    - output command word 1
//! @param pCmd2    - output command word 2
//!
//*****************************************************************************
static inline void
xspimc_lp_cmd_pack_byte(uint32_t *pCmd1, uint32_t *pCmd2,
                        uint32_t ui32Index, uint8_t ui8Byte)
{
    uint32_t ui32Shift = 24u - ((ui32Index & 0x3u) * 8u);

    if (ui32Index < 4u)
    {
        *pCmd1 |= (uint32_t)ui8Byte << ui32Shift;
    }
    else
    {
        *pCmd2 |= (uint32_t)ui8Byte << ui32Shift;
    }
}

static inline uint32_t
am_hal_xspimc_cov_lp_cmd(const am_hal_xspimc_lp_cmdinfo_t *pCmdInfo,
                         uint32_t *pCmd1, uint32_t *pCmd2)
{
    uint32_t ui32Index = 0u;

    if ((pCmdInfo->ui8CmdLen > sizeof(pCmdInfo->ui16Cmd)) ||
        (pCmdInfo->ui8AddrLen > sizeof(pCmdInfo->ui32Address)))
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }

    if (pCmdInfo->ui32NumBytes > (8u - pCmdInfo->ui8CmdLen - pCmdInfo->ui8AddrLen))
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }

    *pCmd1 = 0u;
    *pCmd2 = 0u;

    for (uint32_t i = 0u; i < pCmdInfo->ui8CmdLen; i++)
    {
        uint32_t ui32Shift = (pCmdInfo->ui8CmdLen - 1u - i) * 8u;
        xspimc_lp_cmd_pack_byte(pCmd1, pCmd2, ui32Index++,
                                (uint8_t)(pCmdInfo->ui16Cmd >> ui32Shift));
    }

    for (uint32_t i = 0u; i < pCmdInfo->ui8AddrLen; i++)
    {
        uint32_t ui32Shift = (pCmdInfo->ui8AddrLen - 1u - i) * 8u;
        xspimc_lp_cmd_pack_byte(pCmd1, pCmd2, ui32Index++,
                                (uint8_t)(pCmdInfo->ui32Address >> ui32Shift));
    }

    for (uint32_t i = 0u; i < pCmdInfo->ui32NumBytes; i++)
    {
        xspimc_lp_cmd_pack_byte(pCmd1, pCmd2, ui32Index++,
                                pCmdInfo->pDataBuff[i]);
    }

    return AM_HAL_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Convert low-power configuration into CSLP attribute encoding.
//!
//! @param pCfg  - low power configuration.
//! @param pAttr - output attribute union (CSLPATTR encoding).
//!
//! @return AM_HAL_STATUS_SUCCESS on success; otherwise a HAL status error code.
//
//*****************************************************************************
static inline uint32_t
am_hal_xspimc_cov_lp_attr(const am_hal_xspimc_lp_cfg_t *pCfg,
                          am_hal_xspimc_ll_lp_cmd_attr_t *pAttr)
{
    pAttr->xSPILPCmdAttr = 0u;

    switch (pCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_2_2:
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_4_4:
        case AM_HAL_XSPIMC_IO_1_1_8:
        case AM_HAL_XSPIMC_IO_1_8_8:
            pAttr->xSPILPCmdAttr_b.xSPILPCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_SINGLE;
            break;
        case AM_HAL_XSPIMC_IO_DUAL:
            pAttr->xSPILPCmdAttr_b.xSPILPCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_DUAL;
            break;
        case AM_HAL_XSPIMC_IO_QUAD:
            pAttr->xSPILPCmdAttr_b.xSPILPCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_QUAD;
            break;
        case AM_HAL_XSPIMC_IO_OCTAL:
        case AM_HAL_XSPIMC_IO_8_8_16:
        case AM_HAL_XSPIMC_IO_8_16_16:
            pAttr->xSPILPCmdAttr_b.xSPILPCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_OCTA;
            break;
        case AM_HAL_XSPIMC_IO_HEX:
            pAttr->xSPILPCmdAttr_b.xSPILPCmdMode = AM_HAL_XSPIMC_LL_IO_MODE_HEX;
            break;
        default:
            return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    pAttr->xSPILPCmdAttr_b.xSPILPCmdDTR = xspimc_dtr_cmd(pCfg->eDataRate) ? 1u : 0u;

    if (pCfg->sCmdEntry.ui8CmdLen + pCfg->sCmdEntry.ui8AddrLen + pCfg->sCmdEntry.ui32NumBytes !=
        pCfg->sCmdExit.ui8CmdLen + pCfg->sCmdExit.ui8AddrLen + pCfg->sCmdExit.ui32NumBytes)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }

    // only support sending cmd, need to combine addr and data into two cmd words
    uint32_t ui32ByteLen = pCfg->sCmdEntry.ui8CmdLen +
                           pCfg->sCmdEntry.ui8AddrLen +
                           pCfg->sCmdEntry.ui32NumBytes;
    uint32_t ui32Len    = 0u;
    uint32_t ui32Status = xspimc_len_from_bits((uint64_t)ui32ByteLen * 8ull,
                                               xspimc_lines(pAttr->xSPILPCmdAttr_b.xSPILPCmdMode),
                                               pAttr->xSPILPCmdAttr_b.xSPILPCmdDTR,
                                               0xFu, &ui32Len);
    if (ui32Status)
    {
        return ui32Status;
    }

    pAttr->xSPILPCmdAttr_b.xSPILPCmdLen = ui32Len;

    return AM_HAL_STATUS_SUCCESS;
}

//! @} END_GROUP - xSPI configuration helper functions

extern const am_hal_xspimc_api_t sXSPIAPI;

//*****************************************************************************
//
//! @brief xSPI high level public APIs
//! @defgroup xspimc_api_ap6xx
//! @ingroup ap6xx_hal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief xSPI HAL interface initialization function.
//!
//! @param pHandle      - handle for the module instance.
//!
//! This function accepts an allocated peripheral handle and then
//! initializes the software interface and required resources.
//! The required RAM size can be obtained from `am_hal_xspimc_get_capabilities()`.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_initialize(am_hal_periph_handle_t *pHandle);

//*****************************************************************************
//
//! @brief xSPI HAL interface deinitialization function.
//!
//! @param pHandle      - the handle for the module instance.
//!
//! This function accepts a handle to an instance and de-initializes the
//! interface.
//! It also deinitializes any software resources used.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_deinitialize(am_hal_periph_handle_t *pHandle);

//*****************************************************************************
//
//! @brief xSPI HAL interface enable function.
//!
//! @param pHandle      - the handle for the module instance.
//! @param sCE          - the target device CE.
//!
//! This function accepts a handle to an instance and enables the
//! interface.
//! It acts as a software switch to enable HAL API access.
//! In an RTOS this is typically equivalent to a device open/unlock.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_xspimc_enable(am_hal_periph_handle_t *pHandle,
                                            const am_hal_xspimc_dev_id *pDevice)
{
    AM_HAL_PERIPH_CHK_HANDLE(pHandle)

    const am_hal_xspimc_api_t *pApi = (const am_hal_xspimc_api_t *)pHandle->pApi;
    if (!pApi || !pApi->enable)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    return pApi->enable(pHandle, pDevice);
}

//*****************************************************************************
//
//! @brief xSPI HAL interface disable function.
//!
//! @param pHandle      - the handle for the module instance.
//! @param sCE          - the target device CE.
//!
//! This function accepts a handle to an instance and disables the
//! interface.
//! It acts as a software switch to disable HAL API access.
//! In an RTOS this is typically equivalent to a device close.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_xspimc_disable(am_hal_periph_handle_t *pHandle,
                                             const am_hal_xspimc_dev_id *pDevice)
{
    AM_HAL_PERIPH_CHK_HANDLE(pHandle)

    const am_hal_xspimc_api_t *pApi = (const am_hal_xspimc_api_t *)pHandle->pApi;
    if (!pApi || !pApi->disable)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    return pApi->disable(pHandle, pDevice);
}

//*****************************************************************************
//
//! @brief xSPI power control function.
//!
//! @param pHandle      - handle for the interface.
//! @param sCE          - the target device CE.
//! @param ePowerReq    - the desired power request from peripheral device.
//!
//! This function configures the xSPI power to be FULL, LOW or OFF.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_xspimc_power_control(am_hal_periph_handle_t *pHandle,
                                                   const am_hal_xspimc_dev_id *pDevice,
                                                   am_hal_xspimc_power_req_e ePowerReq)
{
    AM_HAL_PERIPH_CHK_HANDLE(pHandle)

    const am_hal_xspimc_api_t *pApi = (const am_hal_xspimc_api_t *)pHandle->pApi;
    if (!pApi || !pApi->power_control)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    return pApi->power_control(pHandle, pDevice, ePowerReq);
}

//*****************************************************************************
//
//! @brief xSPI configuration function
//!
//! @param pHandle      - handle for the interface.
//! @param sCE          - the target device CE.
//! @param pConfig      - pointer to the configuration structure.
//!
//! This function configures general xSPI settings (software features and
//! controller-wide configuration).
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_xspimc_configure(am_hal_periph_handle_t *pHandle,
                                               const am_hal_xspimc_dev_id *pDevice,
                                               const am_hal_xspimc_cfg_t *pConfig)
{
    AM_HAL_PERIPH_CHK_HANDLE(pHandle)

    const am_hal_xspimc_api_t *pApi = (const am_hal_xspimc_api_t *)pHandle->pApi;
    if (!pApi || !pApi->configure)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    return pApi->configure(pHandle, pDevice, pConfig);
}

//*****************************************************************************
//
//! @brief xSPI device configuration function.
//!
//! @param pHandle      - handle for the interface.
//! @param sCE          - the target device CE.
//! @param pConfig      - pointer to the configuration structure.
//!
//! This function configures the xSPI controller hardware device specific settings
//! for a particular device.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_xspimc_device_configure(am_hal_periph_handle_t *pHandle,
                                                      const am_hal_xspimc_dev_id *pDevice,
                                                      const am_hal_xspimc_dev_cfg_t *pConfig)
{
    AM_HAL_PERIPH_CHK_HANDLE(pHandle)

    const am_hal_xspimc_api_t *pApi = (const am_hal_xspimc_api_t *)pHandle->pApi;
    if (!pApi || !pApi->device_configure)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    return pApi->device_configure(pHandle, pDevice, pConfig);
}

//*****************************************************************************
//
//! @brief xSPI control function.
//!
//! @param pHandle      - handle for the interface.
//! @param sCE          - the target device CE.
//! @param eControl     - control type.
//! @param pConfig      - pointer to the control data (type depends on eControl).
//!
//! This function applies control commands for the xSPI controller such as:
//! AIP configuration, auto power configuration, timing configuration, and
//! callback registration.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_xspimc_control(am_hal_periph_handle_t *pHandle,
                                             const am_hal_xspimc_dev_id *pDevice,
                                             am_hal_xspimc_control_e eControl,
                                             const void *pConfig)
{
    AM_HAL_PERIPH_CHK_HANDLE(pHandle)

    const am_hal_xspimc_api_t *pApi = (const am_hal_xspimc_api_t *)pHandle->pApi;
    if (!pApi || !pApi->control)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    return pApi->control(pHandle, pDevice, eControl, pConfig);
}

//*****************************************************************************
//
//! @brief xSPI transfer function
//!
//! @param pHandle       - handle for the interface.
//! @param sCE           - the target device CE.
//! @param pXfer         - pointer to the transfer control structure.
//!
//! This function performs a transfer on the xSPI using either PIO or DMA.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_xspimc_xfer(am_hal_periph_handle_t *pHandle,
                                          const am_hal_xspimc_dev_id *pDevice,
                                          const am_hal_xspimc_xfer_t *pXfer)
{
    AM_HAL_PERIPH_CHK_HANDLE(pHandle)

    const am_hal_xspimc_api_t *pApi = (const am_hal_xspimc_api_t *)pHandle->pApi;
    if (!pApi || !pApi->xfer)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    return pApi->xfer(pHandle, pDevice, pXfer);
}

//*****************************************************************************
//
//! @brief xSPI status get function
//!
//! @param pHandle       - handle for the interface.
//! @param eType         - the status type to query
//! @param pStatus       - pointer to an output structure for the requested type
//!
//! This function performs status inquiry.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_xspimc_status_get(am_hal_periph_handle_t *pHandle,
                                                am_hal_xspimc_status_type_e eType,
                                                void *pStatus)
{
    AM_HAL_PERIPH_CHK_HANDLE(pHandle)

    const am_hal_xspimc_api_t *pApi = (const am_hal_xspimc_api_t *)pHandle->pApi;
    if (!pApi || !pApi->status)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    return pApi->status(pHandle, eType, pStatus);
}

//*****************************************************************************
//
//! @brief xSPI get capabilities function.
//!
//! @param pHandle      - handle for the interface.
//! @param pCapabilty   - pointer to the capability structure.
//!
//! This function queries the xSPI controller capability table.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_xspimc_get_capabilities(am_hal_periph_handle_t *pHandle,
                                                      am_hal_xspimc_capability_t *pCapabilty)
{
    AM_HAL_PERIPH_CHK_HANDLE(pHandle)

    const am_hal_xspimc_api_t *pApi = (const am_hal_xspimc_api_t *)pHandle->pApi;
    if (!pApi || !pApi->capability)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    return pApi->capability(pHandle, pCapabilty);
}

//! @} END_GROUP - xspi high level public APIs

//*****************************************************************************
//
//! @brief xSPI middle level public APIs
//! @defgroup xspimc_middle_at110
//! @ingroup xspimc_at110
//! @note These functions are stateless: they take `XSPIMC_Type*` and config structs, do
//! input validation, and program hardware only via the LL layer.
//! @{
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief Configure controller-wide bus settings.
//!
//! @param pReg    - xSPI register base pointer.
//! @param pConfig - pointer to the controller configuration.
//!
//! @return status - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_bus_config(XSPIMC_Type *pReg, const am_hal_xspimc_cfg_t *pConfig);

//*****************************************************************************
//
//! @brief Configure device specific controller settings.
//!
//! @param pReg          - xSPI register base pointer.
//! @param sCE           - the target device CE.
//! @param pConfig       - pointer to the device specific controller configuration.
//! @param ui32BusFreqHz - the bus frequency in Hz.
//!
//! @return status - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_dev_config(XSPIMC_Type *pReg, am_hal_xspimc_ce_t sCE,
                                         const am_hal_xspimc_dev_cfg_t *pConfig,
                                         uint32_t ui32BusFreqHz);

//*****************************************************************************
//
//! @brief Configure an AIP (XIP) window.
//!
//! @param pReg    - xSPI register base pointer.
//! @param sCE     - the target device CE.
//! @param pConfig - pointer to the AIP window configuration.
//!
//! @return status  generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_aip_window_config(XSPIMC_Type *pReg, am_hal_xspimc_ce_t sCE,
                                                const am_hal_xspimc_aip_cfg_t *pConfig);

//*****************************************************************************
//
//! @brief Configure automatic suspend/resume behavior for an AIP (XIP) window.
//!
//! Programs the AIP suspend/resume command opcodes and attributes, enables or
//! disables the feature. Timing values are specified in microseconds at the HAL
//! layer; conversion to bus-clock-cycle counts (required by the AIP time
//! registers) is performed by the low-level driver.
//!
//! @param pReg          - xSPI register base pointer.
//! @param sCE           - the target device CE.
//! @param pConfig       - Suspend/resume configuration (times are in microseconds).
//! @param ui32RefHz     - effective bus clock frequency in Hz (must be non-zero).
//!
//! @return AM_HAL_STATUS_SUCCESS on success; otherwise a HAL status error code.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_auto_sus_res_config(XSPIMC_Type *pReg, am_hal_xspimc_ce_t sCE,
                                                  const am_hal_xspimc_sus_res_cfg_t *pConfig,
                                                  uint32_t ui32RefHz);

//*****************************************************************************
//
//! @brief Configure Low power cmds of the controller for a specific device.
//!
//! @param pReg          - xSPI register base pointer.
//! @param sCE           - the target device CE.
//! @param pDevCfg       - pointer to the device configuration.
//! @param pConfig       - pointer to the low power configuration.
//! @param ui32BusFreqHz - the bus frequency in Hz.
//!
//! @return status - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_lp_config(XSPIMC_Type *pReg, am_hal_xspimc_ce_t sCE,
                                        const am_hal_xspimc_dev_cfg_t *pDevCfg,
                                        const am_hal_xspimc_lp_cfg_t *pConfig,
                                        uint32_t ui32BusFreqHz);

//*****************************************************************************
//
//! @brief Configure custom SPI and the selected device's AIP base delay.
//!
//! @param pReg       - xSPI register base pointer.
//! @param sCE        - the target device CE.
//! @param pDelayCfg  - pointer to the delay configuration.
//!
//! Delay values must not exceed IMPDELAY or the 8-bit register fields.
//! Delay bounds are checked before any delay register is changed.
//!
//! @return status    - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_delay_config(XSPIMC_Type *pReg, am_hal_xspimc_ce_t sCE,
                                           const am_hal_xspimc_delay_cfg_t *pDelayCfg);

//*****************************************************************************
//
//! @brief Apply miscellaneous control operations.
//!
//! @param pReg        - xSPI register base pointer.
//! @param sCE         - chip-enable identity selecting the target device.
//! @param ui32Control - am_hal_xspimc_misc_control_e or
//!                     am_hal_xspimc_misc_control_ee operation to apply.
//! @param pConfig     - operation-specific data for extended middle-level
//!                     controls; NULL for high-level forwarding controls.
//!
//! @return status     - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_misc_control(XSPIMC_Type *pReg, am_hal_xspimc_ce_t sCE,
                                           uint32_t ui32Control, void *pConfig);

//*****************************************************************************
//
//! @brief Perform a PIO transfer for a single packet.
//!
//! @param pReg    - xSPI register base pointer.
//! @param pXfer   - transfer-level settings (command/address/mode/dummy).
//! @param pPacket - packet describing direction/address/length/buffer.
//!
//! @return status - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_pio_xfer(XSPIMC_Type *pReg,
                                       const am_hal_xspimc_xfer_t *pXfer,
                                       const am_hal_xspimc_xfer_packet_t *pPacket);

//*****************************************************************************
//
//! @brief Perform a DMA transfer for one packet.
//!
//! @param pReg               - xSPI register base pointer.
//! @param pXfer              - transfer descriptor containing one packets.
//!
//! @return status            - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_dma_xfer_single(XSPIMC_Type *pReg,
                                              const am_hal_xspimc_xfer_t *pXfer);

//*****************************************************************************
//
//! @brief Perform a DMA transfer for multiple packets.
//!
//! @param pReg               - xSPI register base pointer.
//! @param pXfer              - transfer descriptor containing one or more packets.
//! @param pui32DmaCmdList    - DMA command-list buffer (used for multi-packet transfers).
//! @param ui32DmaCmdListSize - size of @p pui32DmaCmdList in bytes.
//! @param bDMAIRQALO         - if true, request an IRQ for each DMA descriptor.
//!
//! @return status            - generic or interface specific status.
//
//*****************************************************************************

extern uint32_t am_hal_xspimc_dma_xfer_linked(XSPIMC_Type *pReg,
                                              const am_hal_xspimc_xfer_t *pXfer,
                                              uint32_t *pui32DmaCmdList,
                                              uint32_t ui32DmaCmdListSize,
                                              bool bDMAIRQALO);


//*****************************************************************************
//
//! @brief Wrapper for am_hal_xspimc_dma_xfer_single and am_hal_xspimc_dma_xfer_linked
//!
//! @param pReg               - xSPI register base pointer.
//! @param pXfer              - transfer descriptor containing one or more packets.
//! @param pui32DmaCmdList    - DMA command-list buffer (used for multi-packet transfers).
//! @param ui32DmaCmdListSize - size of @p pui32DmaCmdList in bytes.
//! @param ui32DmaBurstLen    - DMA burst length selector.
//! @param bDMAIRQALO         - if true, request an IRQ for each DMA descriptor.
//!
//! @return status            - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_dma_xfer(XSPIMC_Type *pReg,
                                       const am_hal_xspimc_xfer_t *pXfer,
                                       uint32_t *pui32DmaCmdList,
                                       uint32_t ui32DmaCmdListSize,
                                       uint32_t ui32DmaBurstLen,
                                       bool bDMAIRQALO);

//*****************************************************************************
//
//! @brief xSPI enable interrupts function
//!
//! @param pReg         - xSPI register base pointer.
//! @param ui32IntMask  - xSPI interrupt mask.
//!
//! This function enables the specific indicated interrupts.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_interrupt_enable(XSPIMC_Type *pReg,
                                               uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief xSPI disable interrupts function
//!
//! @param pReg         - xSPI register base pointer.
//! @param ui32IntMask  - xSPI interrupt mask.
//!
//! This function disables the specific indicated interrupts.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_interrupt_disable(XSPIMC_Type *pReg,
                                                uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief xSPI interrupt status function
//!
//! @param pHandle      - handle for the interface.
//! @param pui32Status  - returns the interrupt status value.
//! @param bEnabledOnly - TRUE: only report interrupt status for enabled interrupts.
//!                       FALSE: report all interrupt status values.
//!
//! This function returns the specific indicated interrupt status.
//!
//! @return status      - interrupt status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_interrupt_status_get(void *pHandle,
                                                   uint32_t  *pui32Status,
                                                   bool bEnabledOnly);

//*****************************************************************************
//
//! @brief xSPI interrupt clear
//!
//! @param pHandle        - handle for the interface.
//! @param ui32IntMask    - uint32_t for interrupts to clear
//!
//! This function clears the interrupts for the given peripheral.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_interrupt_clear(void *pHandle,
                                              uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief xSPI interrupt service routine
//!
//! @param pHandle      - handle for the interface.
//! @param ui32IntStatus - interrupt status.
//!
//! This function is designed to be called from within the user defined ISR
//! in order to service the non-blocking, queued, or DMA processing for a given
//! module instance.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_interrupt_service(void *pHandle,
                                                uint32_t ui32IntStatus);

//! @} END_GROUP - xSPI middle level APIs

//*****************************************************************************
//
//! @brief xSPI Helper Macro.
//! @defgroup xspimc_at110_macros
//! @ingroup xspimc_at110
//! @{
//
//*****************************************************************************
#define IS_POW2(x) (( (x) != 0u ) && ( ((x) & ((x) - 1u)) == 0u ))
#ifndef IS_ALIGNED
#define IS_ALIGNED(base, size)   (((base) & ((size) - 1u)) == 0u)
#endif

#define AM_XSPI_HAL_HANDLE_DEFINE(ATTR, MODULE_NUM, CLKMGR)                                     \
        ATTR const am_hal_xspimc_deps_t xspi ## MODULE_NUM ## Deps = { &(CLKMGR) };             \
        AM_HAL_HANDLE_DEFINE(ATTR __ALIGNED(4), XSPI, MODULE_NUM,                                            \
                             AM_HAL_XSPIMC_GLOBAL_RAM_SIZE,                                     \
                             &sXSPIAPI, &xspi ## MODULE_NUM ## Deps);                           \
        void EXPAND3(am_xspi, MODULE_NUM, _isr)(void)                                           \
        {                                                                                       \
            uint32_t ui32Status;                                                                \
                                                                                                \
            am_hal_xspimc_interrupt_status_get(&AM_HAL_HANDLE_NAME(XSPI, MODULE_NUM),           \
                                               &ui32Status, false);                             \
            am_hal_xspimc_interrupt_clear(&AM_HAL_HANDLE_NAME(XSPI, MODULE_NUM), ui32Status);   \
            am_hal_xspimc_interrupt_service(&AM_HAL_HANDLE_NAME(XSPI, MODULE_NUM), ui32Status); \
        }

#define AM_XSPI_HAL_HANDLE_INITIALIZE(MODULE, DATA, DATASIZE, OPS, DEPS)                        \
        AM_HAL_HANDLE_INITIALIZE(MODULE, DATA, DATASIZE, OPS, &sXSPIAPI, DEPS)

//! @} END_GROUP - xSPI Helper Macro.

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_XSPIMC_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
