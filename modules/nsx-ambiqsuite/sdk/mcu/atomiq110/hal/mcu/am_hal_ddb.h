//*****************************************************************************
//
//! @file am_hal_ddb.h
//!
//! @brief Hardware abstraction for the DBI DSI Bridge.
//!
//! @addtogroup ddb_at110 DDB - DBI DSI Bridge
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

#ifndef AM_HAL_DDB_H
#define AM_HAL_DDB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Configuration types
//
//*****************************************************************************

//*****************************************************************************
//
//! Maximum number of DDB LUT entries.
//
//*****************************************************************************
#define AM_HAL_DDB_LUT_ENTRY_MAX               256U

//*****************************************************************************
//
//! DBI bus width selection.
//
//*****************************************************************************
typedef enum
{
    //
    //! 8-bit DBI data bus.
    //
    AM_HAL_DDB_DBI_WIDTH_8,

    //
    //! 16-bit DBI data bus.
    //
    AM_HAL_DDB_DBI_WIDTH_16
} am_hal_ddb_dbi_width_e;

//*****************************************************************************
//
//! DSI link mode selection.
//!
//! This selects how the DDB drives DSI low-power (LP) and high-speed (HS)
//! transfers. SPLIT, ALL_LP and ALL_HS ignore the LUT link-mode field.
//! Only POLICY selects the mode using the LUT entry.
//
//*****************************************************************************
typedef enum
{
    //
    //! Split mode: commands use LP and pixel data uses HS.
    //
    AM_HAL_DDB_DSI_LINK_MODE_SPLIT = 0,

    //
    //! All DSI transfers use LP mode.
    //
    AM_HAL_DDB_DSI_LINK_MODE_ALL_LP = 1,

    //
    //! All DSI transfers use HS mode.
    //
    AM_HAL_DDB_DSI_LINK_MODE_ALL_HS = 2,

    //
    //! DSI transfer mode is selected by per-command policy.
    //
    AM_HAL_DDB_DSI_LINK_MODE_POLICY = 3,
} am_hal_ddb_dsi_link_mode_e;

//*****************************************************************************
//
//! DDB LUT command packet type.
//
//*****************************************************************************
typedef enum
{
    //
    //! DCS command packet.
    //
    AM_HAL_DDB_LUT_CMD_PKT_TYPE_DCS = 0,

    //
    //! Generic packet.
    //
    AM_HAL_DDB_LUT_CMD_PKT_TYPE_GENERIC = 1,
} am_hal_ddb_lut_cmd_pkt_type_e;

//*****************************************************************************
//
//! DDB LUT command transaction direction.
//
//*****************************************************************************
typedef enum
{
    //
    //! Write transaction.
    //
    AM_HAL_DDB_LUT_CMD_DIR_WRITE = 0,

    //
    //! Read transaction.
    //
    AM_HAL_DDB_LUT_CMD_DIR_READ = 1,
} am_hal_ddb_lut_cmd_dir_e;

//*****************************************************************************
//
//! DSI link mode for LUT commands.
//!
//! Effective only when the global link mode is AM_HAL_DDB_DSI_LINK_MODE_POLICY.
//! In that mode, value 2 selects HS and values 0 and 1 select LP.
//
//*****************************************************************************
typedef enum
{
    //
    //! Retained name for LUT value 0. Selects LP in global POLICY mode.
    //! In other global modes, the LUT field is ignored.
    //
    AM_HAL_DDB_LUT_CMD_LINK_MODE_USE_GLOBAL = 0,

    //
    //! Select LP for this command when the global link mode is POLICY.
    //
    AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_LP = 1,

    //
    //! Select HS for this command when the global link mode is POLICY.
    //
    AM_HAL_DDB_LUT_CMD_LINK_MODE_FORCE_HS = 2,

    //
    //! Reserved link mode value.
    //
    AM_HAL_DDB_LUT_CMD_LINK_MODE_RESERVED = 3,
} am_hal_ddb_lut_cmd_link_mode_e;

//*****************************************************************************
//
//! Read response length mode for LUT commands.
//!
//! This field selects the length source; a zero length does not select another
//! source. Only GLOBAL_FIXED uses the global RDLEN register.
//
//*****************************************************************************
typedef enum
{
    //
    //! Use the WC field from the DSI RX packet header.
    //
    AM_HAL_DDB_LUT_CMD_READ_LEN_USE_DSI_WC = 0,

    //
    //! Use this command's fixed read length field.
    //
    AM_HAL_DDB_LUT_CMD_READ_LEN_PER_CMD_FIXED = 1,

    //
    //! Use the global fixed read length setting.
    //
    AM_HAL_DDB_LUT_CMD_READ_LEN_GLOBAL_FIXED = 2,
} am_hal_ddb_lut_cmd_read_len_mode_e;

//*****************************************************************************
//
//! DSI virtual channel selection.
//
//*****************************************************************************
typedef enum
{
    //
    //! Virtual channel 0.
    //
    AM_HAL_DDB_DSI_VC_0 = 0,

    //
    //! Virtual channel 1.
    //
    AM_HAL_DDB_DSI_VC_1 = 1,

    //
    //! Virtual channel 2.
    //
    AM_HAL_DDB_DSI_VC_2 = 2,

    //
    //! Virtual channel 3.
    //
    AM_HAL_DDB_DSI_VC_3 = 3,
} am_hal_ddb_dsi_vc_e;

//*****************************************************************************
//
//! DDB command segmentation configuration.
//
//*****************************************************************************
typedef enum
{
    AM_HAL_DDB_CONT_EN = 1,
    AM_HAL_DDB_FLUSH_PARTIAL_ON_BOUNDARY = 2,
    AM_HAL_DDB_FORCE_CONT_CMD_ALWAYS = 4,
} am_hal_ddb_cont_ctrl_e;
//*****************************************************************************
//
//! DDB control request.
//
//*****************************************************************************
typedef enum
{
    //
    //! Issue a DDB soft reset.
    //
    AM_HAL_DDB_REQ_SOFT_RESET = 0,

    //
    //! Enable DBIB_STALL backpressure.
    //
    AM_HAL_DDB_REQ_STALL_ENABLE,

    //
    //! Disable DBIB_STALL backpressure.
    //
    AM_HAL_DDB_REQ_STALL_DISABLE,

    //
    //! Enable DBIB_DV output.
    //
    AM_HAL_DDB_REQ_DV_ENABLE,

    //
    //! Disable DBIB_DV output.
    //
    AM_HAL_DDB_REQ_DV_DISABLE,

    //
    //! Disable the DDB APB clock.
    //
    AM_HAL_DDB_REQ_APBCLK_DISABLE,

    //
    //! Enable the DDB APB clock.
    //
    AM_HAL_DDB_REQ_APBCLK_ENABLE,

    //
    //! Set the static DSI virtual channel.
    //
    AM_HAL_DDB_REQ_VC_SET,

    //
    //! Set max return packet size. pArgs points to
    //! am_hal_ddb_max_return_packet_config_t.
    //
    AM_HAL_DDB_REQ_MAX_RETURN_PACKET_SET,

    //
    //! Get DDB debug counters (STATTX / STATRX / STATRD).
    //! pArgs points to am_hal_ddb_debug_counters_t.
    //
    AM_HAL_DDB_REQ_DEBUG_COUNTERS_GET,

    //
    //! Get DDB sticky error flags. pArgs points to am_hal_ddb_error_flags_t.
    //
    AM_HAL_DDB_REQ_ERROR_FLAGS_GET,

    //
    //! Clear DDB sticky error flags. pArgs points to am_hal_ddb_error_flags_t.
    //
    AM_HAL_DDB_REQ_ERROR_FLAGS_CLEAR,

    //
    //! Get DDB peripheral ID. pArgs points to uint32_t.
    //
    AM_HAL_DDB_REQ_ID_GET,

    //
    //! Get DDB device-idle indicator. pArgs points to bool.
    //
    AM_HAL_DDB_REQ_DEVICE_IDLE_GET,

    //
    //! Request count.
    //
    AM_HAL_DDB_REQ_MAX,
} am_hal_ddb_request_e;

//*****************************************************************************
//
//! DDB LUT command attributes.
//!
//! This structure describes how a DBI command matched by the LUT is translated
//! into a DSI transaction.
//!
//! For valid entries, all four DV policy fields are taken directly from the
//! LUT. They do not inherit the global RDDV fields. An invalid entry raises
//! LUT_MISS and discards the transaction.
//
//*****************************************************************************
typedef struct
{
    union
    {
        __IOM uint32_t ui32LUTWR0;
        struct
        {
            __IOM uint32_t ui32Valid : 1;
            __IOM uint32_t ui32IsGeneric : 1;
            __IOM uint32_t ui32IsRead : 1;
            __IOM uint32_t ui32IsContinuable : 1;
            __IOM uint32_t ui32AutoContinueEn : 1;
            __IOM uint32_t ui32Dbi16CmdHiIsParam : 1;
            __IOM uint32_t ui32ModePolicy : 2;
            __IOM uint32_t ui32ContinueCmd : 8;
            __IOM uint32_t ui32SegNBytes : 16;
        } ui32LUTWR0_b;
    };

    union
    {
        __IOM uint32_t ui32LUTWR1;
        struct
        {
            __IOM uint32_t ui32ReadLenMode : 2;
            __IOM uint32_t ui32ReadLenFixedBytes : 16;
            __IOM uint32_t ui32RDDVDummyRDX : 8;
            __IOM uint32_t ui32RDDVWaitFull : 1;
            __IOM uint32_t ui32RDDVGapOK : 1;
            __IOM uint32_t ui32RDDVStartTHBytes : 4;
        } ui32LUTWR1_b;
    };

    union
    {
        __IOM uint32_t ui32LUTWR2;
        struct
        {
            __IOM uint32_t ui32RDDVStartTHBytes : 12;
            uint32_t : 20;
        } ui32LUTWR2_b;
    };
} am_hal_ddb_lut_cmd_attr_t;

//*****************************************************************************
//
//! Default LUT command attributes.
//
//*****************************************************************************

#define AM_HAL_DDB_LUT_CMD_ATTR_DCS_WRITE_DEFAULT \
    { \
        .ui32LUTWR0_b.ui32Valid                           = true, \
        .ui32LUTWR0_b.ui32IsGeneric                       = AM_HAL_DDB_LUT_CMD_PKT_TYPE_DCS, \
        .ui32LUTWR0_b.ui32IsRead                          = AM_HAL_DDB_LUT_CMD_DIR_WRITE, \
        .ui32LUTWR0_b.ui32IsContinuable                   = false, \
        .ui32LUTWR0_b.ui32AutoContinueEn                  = false, \
        .ui32LUTWR0_b.ui32Dbi16CmdHiIsParam               = false, \
        .ui32LUTWR0_b.ui32AutoContinueEn               = false, \
        .ui32LUTWR0_b.ui32ModePolicy                        = AM_HAL_DDB_LUT_CMD_LINK_MODE_USE_GLOBAL, \
        .ui32LUTWR0_b.ui32ContinueCmd                    = 0U, \
        .ui32LUTWR0_b.ui32SegNBytes                      = 0U, \
        .ui32LUTWR1_b.ui32ReadLenMode                     = AM_HAL_DDB_LUT_CMD_READ_LEN_USE_DSI_WC, \
        .ui32LUTWR1_b.ui32ReadLenFixedBytes            = 0U, \
        .ui32LUTWR1_b.ui32RDDVDummyRDX                = 0U, \
        .ui32LUTWR1_b.ui32RDDVWaitFull                  = false, \
        .ui32LUTWR1_b.ui32RDDVGapOK                     = false, \
        .ui32LUTWR1_b.ui32RDDVStartTHBytes           = 0U, \
        .ui32LUTWR2_b.ui32RDDVStartTHBytes           = 0U, \
    }

#define AM_HAL_DDB_LUT_CMD_ATTR_DCS_READ_DEFAULT \
    { \
        .ui32LUTWR0_b.ui32Valid                           = true, \
        .ui32LUTWR0_b.ui32IsGeneric                         = AM_HAL_DDB_LUT_CMD_PKT_TYPE_DCS, \
        .ui32LUTWR0_b.ui32IsRead                       = AM_HAL_DDB_LUT_CMD_DIR_READ, \
        .ui32LUTWR0_b.ui32IsContinuable                   = false, \
        .ui32LUTWR0_b.ui32AutoContinueEn                  = false, \
        .ui32LUTWR0_b.ui32Dbi16CmdHiIsParam               = false, \
        .ui32LUTWR0_b.ui32ModePolicy                        = AM_HAL_DDB_LUT_CMD_LINK_MODE_USE_GLOBAL, \
        .ui32LUTWR0_b.ui32ContinueCmd                   = 0U, \
        .ui32LUTWR0_b.ui32SegNBytes                 = 0U, \
        .ui32LUTWR1_b.ui32ReadLenMode                     = AM_HAL_DDB_LUT_CMD_READ_LEN_USE_DSI_WC, \
        .ui32LUTWR1_b.ui32ReadLenFixedBytes            = 0U, \
        .ui32LUTWR1_b.ui32RDDVDummyRDX                = 1U, \
        .ui32LUTWR1_b.ui32RDDVWaitFull                  = false, \
        .ui32LUTWR1_b.ui32RDDVGapOK                     = false, \
        .ui32LUTWR1_b.ui32RDDVStartTHBytes    = 1U, \
        .ui32LUTWR2_b.ui32RDDVStartTHBytes           = 0U, \
    }

#define AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_WRITE_DEFAULT \
    { \
        .ui32LUTWR0_b.ui32Valid                           = true, \
        .ui32LUTWR0_b.ui32IsGeneric                         = AM_HAL_DDB_LUT_CMD_PKT_TYPE_GENERIC, \
        .ui32LUTWR0_b.ui32IsRead                       = AM_HAL_DDB_LUT_CMD_DIR_WRITE, \
        .ui32LUTWR0_b.ui32IsContinuable                   = false, \
        .ui32LUTWR0_b.ui32AutoContinueEn                  = false, \
        .ui32LUTWR0_b.ui32Dbi16CmdHiIsParam               = false, \
        .ui32LUTWR0_b.ui32ModePolicy                        = AM_HAL_DDB_LUT_CMD_LINK_MODE_USE_GLOBAL, \
        .ui32LUTWR0_b.ui32ContinueCmd                   = 0U, \
        .ui32LUTWR0_b.ui32SegNBytes                 = 0U, \
        .ui32LUTWR1_b.ui32ReadLenMode                     = AM_HAL_DDB_LUT_CMD_READ_LEN_USE_DSI_WC, \
        .ui32LUTWR1_b.ui32ReadLenFixedBytes            = 0U, \
        .ui32LUTWR1_b.ui32RDDVDummyRDX                = 0U, \
        .ui32LUTWR1_b.ui32RDDVWaitFull                  = false, \
        .ui32LUTWR1_b.ui32RDDVGapOK                     = false, \
        .ui32LUTWR1_b.ui32RDDVStartTHBytes    = 0U, \
        .ui32LUTWR2_b.ui32RDDVStartTHBytes           = 0U, \
    }

#define AM_HAL_DDB_LUT_CMD_ATTR_GENERIC_READ_DEFAULT \
    { \
        .ui32LUTWR0_b.ui32Valid                           = true, \
        .ui32LUTWR0_b.ui32IsGeneric                         = AM_HAL_DDB_LUT_CMD_PKT_TYPE_GENERIC, \
        .ui32LUTWR0_b.ui32IsRead                       = AM_HAL_DDB_LUT_CMD_DIR_READ, \
        .ui32LUTWR0_b.ui32IsContinuable                   = false, \
        .ui32LUTWR0_b.ui32AutoContinueEn                  = false, \
        .ui32LUTWR0_b.ui32Dbi16CmdHiIsParam               = false, \
        .ui32LUTWR0_b.ui32ModePolicy                        = AM_HAL_DDB_LUT_CMD_LINK_MODE_USE_GLOBAL, \
        .ui32LUTWR0_b.ui32ContinueCmd                   = 0U, \
        .ui32LUTWR0_b.ui32SegNBytes                 = 0U, \
        .ui32LUTWR1_b.ui32ReadLenMode                     = AM_HAL_DDB_LUT_CMD_READ_LEN_USE_DSI_WC, \
        .ui32LUTWR1_b.ui32ReadLenFixedBytes            = 0U, \
        .ui32LUTWR1_b.ui32RDDVDummyRDX                = 1U, \
        .ui32LUTWR1_b.ui32RDDVWaitFull                  = false, \
        .ui32LUTWR1_b.ui32RDDVGapOK                     = false, \
        .ui32LUTWR1_b.ui32RDDVStartTHBytes    = 1U, \
        .ui32LUTWR2_b.ui32RDDVStartTHBytes           = 0U, \
    }

//! @}


//*****************************************************************************
//
//! DDB LUT command configuration.
//
//*****************************************************************************
typedef struct
{
    //
    //! Command index in the 256-entry LUT.
    //
    uint32_t ui32LUTWRAddr;

    //
    //! Attributes for this command.
    //
    am_hal_ddb_lut_cmd_attr_t sCmdAttr;
} am_hal_ddb_lut_config_t;

//*****************************************************************************
//
//! DDB Set Max Return Packet Size configuration.
//
//*****************************************************************************
typedef union
{
    __IOM uint32_t ui32RDMisc;
    struct
    {
        __IOM uint32_t ui32SetMaxRetPktEN : 1;
        __IOM uint32_t ui32MaxRetPktBytes : 15;
        uint32_t : 16;
    } ui32RDMisc_b;
} am_hal_ddb_max_return_packet_config_t;

//*****************************************************************************
//
//! DDB configuration.
//
//*****************************************************************************
typedef struct
{
    union
    {
        __IOM uint32_t ui32DBI16;
        struct
        {
            __IOM uint32_t ui32DBI16 : 1;
            __IOM uint32_t ui32DBI16ByteSwap : 1;
            uint32_t : 30;
        } ui32DBI16_b;
    };

    __IOM uint32_t ui32LinkMode;

    __IOM uint32_t ui32VC;

    union
    {
        __IOM uint32_t ui32ContCtrl;
        struct
        {
            __IOM uint32_t ui32ContEN : 1;
            __IOM uint32_t ui32FlushPartialonBoundary : 1;
            __IOM uint32_t ui32ForceContCmdAlways : 1;
            uint32_t : 29;
        } ui32ContCtrl_b;
    };

    __IOM uint32_t ui32ContNBytes;

} am_hal_ddb_config_t;

//*****************************************************************************
//
//! STATTX debug counters.
//
//*****************************************************************************
typedef struct
{
    //
    //! TX packet count (STATTX.TXPKTCNT).
    //
    uint16_t ui16PacketCount;

    //
    //! TX payload byte count (STATTX.TXPLDCNT).
    //
    uint16_t ui16PayloadCount;
} am_hal_ddb_debug_stattx_t;

//*****************************************************************************
//
//! STATRX debug counters.
//
//*****************************************************************************
typedef struct
{
    //
    //! RX packet count (STATRX.RXPKTCNT).
    //
    uint16_t ui16PacketCount;

    //
    //! RX payload byte count (STATRX.RXPLDCNT).
    //
    uint16_t ui16PayloadCount;
} am_hal_ddb_debug_statrx_t;

//*****************************************************************************
//
//! STATRD debug counters.
//
//*****************************************************************************
typedef struct
{
    //
    //! DBI read bytes delivered (STATRD.RXBYTECNT).
    //
    uint16_t ui16RxByteCount;

    //
    //! DBI read transaction count (STATRD.RDCNT).
    //
    uint16_t ui16ReadCount;
} am_hal_ddb_debug_statrd_t;

//*****************************************************************************
//
//! DDB debug counters (STATTX / STATRX / STATRD).
//
//*****************************************************************************
typedef struct
{
    //
    //! STATTX counters.
    //
    am_hal_ddb_debug_stattx_t sStatTx;

    //
    //! STATRX counters.
    //
    am_hal_ddb_debug_statrx_t sStatRx;

    //
    //! STATRD counters.
    //
    am_hal_ddb_debug_statrd_t sStatRd;
} am_hal_ddb_debug_counters_t;

//*****************************************************************************
//
//! DDB sticky error flags.
//
//*****************************************************************************
typedef union
{
    //
    //! Complete error flag mask.
    //
    uint32_t ui32ErrFlag;

    struct
    {
        uint32_t ui32TxPayOverflow     : 1;    // TX payload FIFO overflow.
        uint32_t ui32TxDescOverflow    : 1;    // TX descriptor FIFO overflow.
        uint32_t ui32RxByteOverflow    : 1;    // RX byte FIFO overflow.
        uint32_t ui32RxMetaOverflow    : 1;    // RX meta FIFO overflow.
        uint32_t ui32TagMismatch       : 1;    // RX meta tag mismatch.
        uint32_t ui32LutMiss           : 1;    // Command byte missed in LUT.
        uint32_t ui32SegZeroLen        : 1;    // Effective segment size is 0.
        uint32_t ui32StallTimeout      : 1;    // Backpressure stall timeout.
        uint32_t ui32BtaTimeout        : 1;    // BTA timeout.
        uint32_t ui32RxEcc1Bit         : 1;    // Corrected RX header ECC error.
        uint32_t ui32RxEcc2Bit         : 1;    // Uncorrectable RX header ECC error.
        uint32_t ui32RxCrcErr          : 1;    // RX payload CRC error.
        uint32_t ui32HsTxTimeout       : 1;    // HS transmit timeout.
        uint32_t ui32LpRxTimeout       : 1;    // LP RX timeout.
        uint32_t ui32Reserved       : 18;   // Reserved, write as 0.
    } ui32ErrFlag_b;
} am_hal_ddb_error_flags_t;

//*****************************************************************************
//
// Function prototypes
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief DDB initialization function.
//!
//! @param ui32Module   - module instance.
//! @param ppHandle     - returns the handle for the module instance.
//!
//! Initialize a DDB instance and return its handle.
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_initialize(uint32_t ui32Module, void **ppHandle);

//*****************************************************************************
//
//! @brief DDB deinitialization function.
//!
//! @param pHandle      - handle for the module instance.
//!
//! Deinitialize a DDB instance.
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_deinitialize(void *pHandle);

//*****************************************************************************
//
//! @brief DDB enable function.
//!
//! @param pHandle      - handle for the module instance.
//!
//! Enable the DDB for operation.
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_power_control(void *pHandle,
                                         am_hal_sysctrl_power_state_e ePowerState,
                                         bool bRetainState);

//*****************************************************************************
//
//! @brief DDB enable function.
//!
//! @param pHandle      - handle for the module instance.
//!
//! Enable the DDB for operation.
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_enable(void *pHandle);

//*****************************************************************************
//
//! @brief DDB disable function.
//!
//! @param pHandle      - handle for the module instance.
//!
//! Disable the DDB from operation.
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_disable(void *pHandle);

//*****************************************************************************
//
//! @brief DDB control function.
//!
//! @param pHandle      - handle for the module instance.
//! @param eReq         - DDB control request.
//! @param pArgs        - optional request parameters.
//!
//! Perform a DDB control operation.
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_control(void *pHandle,
                                   am_hal_ddb_request_e eReq,
                                   void *pArgs);

//*****************************************************************************
//
//! @brief DDB configuration function.
//!
//! @param pHandle      - handle for the module instance.
//! @param psConfig     - pointer to the DDB configuration.
//!
//! Configure the DDB interface settings.
//! DBI16 input byte swap is controlled separately with
//! AM_HAL_DDB_REQ_DBI16_BYTE_SWAP_ENABLE / DISABLE and is preserved here.
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_configure(void *pHandle,
                                     const am_hal_ddb_config_t *psConfig);

//*****************************************************************************
//
//! @brief Configure DDB LUT entries.
//!
//! @param pHandle        - handle for the module instance.
//! @param psConfig       - pointer to an array of LUT configurations.
//! @param ui32NumEntries - number of entries in psConfig.
//!
//! Configure one or more LUT command entries.
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_lut_configure(void *pHandle,
                                         const am_hal_ddb_lut_config_t *psConfig,
                                         uint32_t ui32NumEntries);

//*****************************************************************************
//
//! @brief Read a DDB LUT command entry.
//!
//! @param pHandle      - handle for the module instance.
//! @param psConfig     - LUT command configuration.
//!
//! Read the LUT entry selected by psConfig->ui8Cmd via LUTRDADDR/LUTR0-2 and
//! return its attributes in psConfig->sCmdAttr.
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_lut_read(void *pHandle,
                                    am_hal_ddb_lut_config_t *psConfig,
                                    uint32_t ui32NumEntries);

//*****************************************************************************
//
//! @brief Get DDB device-idle indicator.
//!
//! @param pHandle      - handle for the module instance.
//! @param pbDeviceIdle - returns true when DEVICEIDLE is set.
//!
//! Reads DBIDSIBRIDGE DEVICEIDLE (bridge internal idle indicator).
//!
//! @return HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_ddb_device_idle_get(void *pHandle, bool *pbDeviceIdle);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_DDB_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
