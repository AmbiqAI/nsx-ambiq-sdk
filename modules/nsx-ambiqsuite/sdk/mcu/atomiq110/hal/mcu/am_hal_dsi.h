//*****************************************************************************
//
//! @file am_hal_dsi.h
//!
//! @brief Functions and definitions for the MIPI DSI HAL.
//!
//! This module provides an interface for configuring and controlling the
//! MIPI Display Serial Interface (DSI) hardware.
//!
//! @addtogroup dsi_atomiq110 DSI Functionality
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
#ifndef AM_HAL_DSI_H
#define AM_HAL_DSI_H

#include <stdint.h>
#include <stdbool.h>

#include "hal/am_hal_sysctrl.h"

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************

//
//! periph_status port of the Host Controller
//
#define AM_HAL_DSI_PERIPH_STATUS_SOT_ERROR             DSI_PERIPHSTATUS_CFGSTATUSBIT0_Msk  //!< Start of Transmission Error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_SOT_SYNC_ERROR        DSI_PERIPHSTATUS_CFGSTATUSBIT1_Msk  //!< Start of Transmission Sync Error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_EOT_SYNC_ERROR        DSI_PERIPHSTATUS_CFGSTATUSBIT2_Msk  //!< End of Transmission Sync Error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_ESCAPE_ENTRY_ERROR    DSI_PERIPHSTATUS_CFGSTATUSBIT3_Msk  //!< Escape Mode Entry Command Error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_LP_TX_SYNC_ERROR      DSI_PERIPHSTATUS_CFGSTATUSBIT4_Msk  //!< Low Power Transmit Sync error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_PERIPH_TIMEOUT_ERROR  DSI_PERIPHSTATUS_CFGSTATUSBIT5_Msk  //!< Peripheral Timeout error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_FALSE_CONTROL_ERROR   DSI_PERIPHSTATUS_CFGSTATUSBIT6_Msk  //!< False Control Error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_CONTENTION_DETECTED   DSI_PERIPHSTATUS_CFGSTATUSBIT7_Msk  //!< Contention Detection from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_SINGLE_BIT_ECC_ERROR  DSI_PERIPHSTATUS_CFGSTATUSBIT8_Msk  //!< ECC single bit error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_MULTI_BIT_ECC_ERROR   DSI_PERIPHSTATUS_CFGSTATUSBIT9_Msk  //!< ECC multi-bit error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_CHECKSUM_ERROR        DSI_PERIPHSTATUS_CFGSTATUSBIT10_Msk //!< Checksum error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_DT_NOT_RECOGNIZED     DSI_PERIPHSTATUS_CFGSTATUSBIT11_Msk //!< Invalid data type from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_VC_ID_INVALID         DSI_PERIPHSTATUS_CFGSTATUSBIT12_Msk //!< Invalid VC from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_INVALID_TX_LENGTH     DSI_PERIPHSTATUS_CFGSTATUSBIT13_Msk //!< Transmission length error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_PROTOCOL_VIOLATION    DSI_PERIPHSTATUS_CFGSTATUSBIT15_Msk //!< Protocol violation error from peripheral error report, cleared upon read
#define AM_HAL_DSI_PERIPH_STATUS_RESET_TRIGGER         DSI_PERIPHSTATUS_CFGSTATUSBIT16_Msk //!< Reset trigger (01100010)
#define AM_HAL_DSI_PERIPH_STATUS_UNKNOWN_TRIGGER_3     DSI_PERIPHSTATUS_CFGSTATUSBIT17_Msk //!< Unknown trigger 3 (01011101)
#define AM_HAL_DSI_PERIPH_STATUS_UNKNOWN_TRIGGER_4     DSI_PERIPHSTATUS_CFGSTATUSBIT18_Msk //!< Unknown trigger 4 (00100001)
#define AM_HAL_DSI_PERIPH_STATUS_UNKNOWN_TRIGGER_5     DSI_PERIPHSTATUS_CFGSTATUSBIT19_Msk //!< Unknown trigger 5 (10100000)

//
//! Host receive error detection.
//
#define AM_HAL_DSI_RX_ERROR_STATUS_SINGLE_BIT_ECC_ERROR          (1UL << 0)  //!< ECC single bit error
#define AM_HAL_DSI_RX_ERROR_STATUS_MULTI_BIT_ECC_ERROR           (1UL << 1)  //!< ECC multi-bit error
#define AM_HAL_DSI_RX_ERROR_STATUS_SINGLE_BIT_ECC_ERROR_POSITION (1UL << 2)  //!< Bit position for ECC single bit error
#define AM_HAL_DSI_RX_ERROR_STATUS_CRC_ERROR                     (1UL << 7)  //!< CRC error
#define AM_HAL_DSI_RX_ERROR_STATUS_HS_TX_TIMEOUT                 (1UL << 8)  //!< High Speed forward TX timeout
#define AM_HAL_DSI_RX_ERROR_STATUS_LP_RX_TIMEOUT                 (1UL << 9)  //!< Reverse Low Power Data receive timeout
#define AM_HAL_DSI_RX_ERROR_STATUS_BTA_TIMEOUT                   (1UL << 10) //!< BTA timeout

//
//! Status of the APB packet interface & APB packet interface IRQ.
//! The corresponding mask bit of CFG_IRQ_MASK.
//
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_STATE_MACHINE_NOT_IDLE       (1UL << 0)  //!< 1'b0: State machine is idle. 1'b1: State machine is not idle
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_TX_PKT_TRANSMITTED           (1UL << 1)  //!< Tx packet has been transmitted. Clears at next request.
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_CURRENT_PHY_DIRECTION        (1UL << 2)  //!< 0 - Host side has control. 1 - Peripheral has control.
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_TX_PAYLOAD_FIFO_OVERFLOW     (1UL << 3)  //!< 1'b1: Tx Payload FIFO has overflowed. Clears to 1'b0 upon read
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_TX_PAYLOAD_FIFO_UNDERFLOW    (1UL << 4)  //!< 1'b1: Tx Payload FIFO has underflowed. Clears to 1'b0 upon read
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_RX_PAYLOAD_FIFO_OVERFLOW     (1UL << 5)  //!< 1'b1: Rx Payload FIFO has overflowed. Clears to 1'b0 upon read
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_RX_PAYLOAD_FIFO_UNDERFLOW    (1UL << 6)  //!< 1'b1: Rx Payload FIFO has underflowed. Clears to 1'b0 upon read
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_RX_PKT_HEADER_RECEIVED       (1UL << 7)  //!< Rx packet headers has been received. Clears at next request.
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_ALL_RX_PKT_PAYLOAD_RECEIVED  (1UL << 8)  //!< All Rx packet payload has been received. Clears at next request.

//
//! Status of the APB packet interface IRQ only.
//! The corresponding mask bit of CFG_IRQ_MASK.
//
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_SOT_ERROR                    (AM_HAL_DSI_PERIPH_STATUS_SOT_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_SOT_SYNC_ERROR               (AM_HAL_DSI_PERIPH_STATUS_SOT_SYNC_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_EOT_SYNC_ERROR               (AM_HAL_DSI_PERIPH_STATUS_EOT_SYNC_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_ESCAPE_ENTRY_ERROR           (AM_HAL_DSI_PERIPH_STATUS_ESCAPE_ENTRY_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_LP_TX_SYNC_ERROR             (AM_HAL_DSI_PERIPH_STATUS_LP_TX_SYNC_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_PERIPH_TIMEOUT_ERROR         (AM_HAL_DSI_PERIPH_STATUS_PERIPH_TIMEOUT_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_FALSE_CONTROL_ERROR          (AM_HAL_DSI_PERIPH_STATUS_FALSE_CONTROL_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_CONTENTION_DETECTED          (AM_HAL_DSI_PERIPH_STATUS_CONTENTION_DETECTED << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_SINGLE_BIT_ECC_ERROR         (AM_HAL_DSI_PERIPH_STATUS_SINGLE_BIT_ECC_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_MULTI_BIT_ECC_ERROR          (AM_HAL_DSI_PERIPH_STATUS_MULTI_BIT_ECC_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_CHECKSUM_ERROR               (AM_HAL_DSI_PERIPH_STATUS_CHECKSUM_ERROR << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_DT_NOT_RECOGNIZED            (AM_HAL_DSI_PERIPH_STATUS_DT_NOT_RECOGNIZED << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_VC_ID_INVALID                (AM_HAL_DSI_PERIPH_STATUS_VC_ID_INVALID << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_INVALID_TX_LENGTH            (AM_HAL_DSI_PERIPH_STATUS_INVALID_TX_LENGTH << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_PROTOCOL_VIOLATION           (AM_HAL_DSI_PERIPH_STATUS_PROTOCOL_VIOLATION << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_RESET_TRIGGER                (AM_HAL_DSI_PERIPH_STATUS_RESET_TRIGGER << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_UNKNOWN_TRIGGER_3            (AM_HAL_DSI_PERIPH_STATUS_UNKNOWN_TRIGGER_3 << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_UNKNOWN_TRIGGER_4            (AM_HAL_DSI_PERIPH_STATUS_UNKNOWN_TRIGGER_4 << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_UNKNOWN_TRIGGER_5            (AM_HAL_DSI_PERIPH_STATUS_UNKNOWN_TRIGGER_5 << 9)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_HOST_BTA_TIMEOUT             (1UL << 29)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_LP_RX_TIMEOUT                (1UL << 30)
#define AM_HAL_DSI_PKT_IRQ_STATUS_OR_MASK_HS_TX_TIMEOUT                (1UL << 31)

//
//! Status of the APB packet interface IRQ part 2.
//! The corresponding mask bit of CFG_IRQ_MASK2.
//
#define AM_HAL_DSI_PKT_IRQ_STATUS2_OR_MASK2_1_BIT_ECC_ERROR            (1UL << 0)
#define AM_HAL_DSI_PKT_IRQ_STATUS2_OR_MASK2_2_BIT_ECC_ERROR            (1UL << 1)
#define AM_HAL_DSI_PKT_IRQ_STATUS2_OR_MASK2_CRC_ERROR                  (1UL << 2)

//*****************************************************************************
//
// Type definitions
//
//*****************************************************************************

//
//! DSI Transfer callback function prototype.
//
typedef void (*am_hal_dsi_callback_t)(void *pCallbackCtxt, uint32_t status);

//
// DSI video mode formats
//
typedef enum
{
// #### INTERNAL BEGIN ####
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_COMPRESSED = 0x0B,
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_YCBCR_20BIT_LP = 0x0C,
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_YCBCR_24BIT = 0x1C,
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_YCBCR_16BIT = 0x2C,
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_YCBCR_20BIT = 0x3C,
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_RGB_101010_30BIT = 0x0D,
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_RGB_121212_36BIT = 0x1D,
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_YCBCR_12BIT = 0x3D,
// #### INTERNAL END ####
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_RGB_565_16BIT = 0x0E,
// #### INTERNAL BEGIN ####
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_RGB_666_18BIT = 0x1E,
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_RGB_666_18BIT_LP = 0x2E,
// #### INTERNAL END ####
    AM_HAL_DSI_VIDEO_PIXEL_FORMAT_RGB_888_24BIT = 0x3E
}
am_hal_dsi_video_pixel_format_e;

//
//! DSI video mode types
//
typedef enum
{
    AM_HAL_DSI_VIDEO_MODE_NON_BURST_SYNC_PULSE,
    AM_HAL_DSI_VIDEO_MODE_NON_BURST_SYNC_EVENTS,
    AM_HAL_DSI_VIDEO_MODE_BURST
}
am_hal_dsi_video_mode_e;

//
//! DSI video mode timings
//
typedef struct
{
    uint16_t ui16VidHACT;   //!< Horizontal Active Area in pixel clock cycles.
    uint16_t ui16VidHFP;    //!< Horizontal Front Porch in pixel clock cycles.
    uint16_t ui16VidHBP;    //!< Horizontal Back Porch in pixel clock cycles.
    uint16_t ui16VidHSA;    //!< Horizontal Sync Active in pixel clock cycles.
    uint16_t ui16VidVBP;    //!< Vertical Back Porch in lines.
    uint16_t ui16VidVFP;    //!< Vertical Front Porch in lines.
    uint16_t ui16VidVSA;    //!< Vertical Sync Active in lines.
    uint16_t ui16VidVACT;   //!< Vertical Active Area in lines.
}
am_hal_dsi_video_timing_t;

//
//! DSI video BLLP mode.
//
typedef enum
{
    AM_HAL_DSI_VIDEO_BLLP_MODE_BLANKING_H_BLANKING_V = 0,  //!< Blanking packets during both horizontal and vertical BLLP periods
    AM_HAL_DSI_VIDEO_BLLP_MODE_BLANKING_H_LP_V,            //!< LP mode for vertical BLLP periods
    AM_HAL_DSI_VIDEO_BLLP_MODE_LP_H_BLANKING_V,            //!< Blanking packets during horizontal BLLP periods
    AM_HAL_DSI_VIDEO_BLLP_MODE_LP_H_LP_V                   //!< LP mode for both horizontal and vertical BLLP periods
}
am_hal_dsi_video_bllp_mode_e;

//
// DSI HS timings & PLL configuration
//
typedef struct
{
    union
    {
        __IOM uint32_t ui32DataLaneHS;
        struct
        {
            __IOM uint32_t ui32ClockLaneHSPreTime : 8;
            __IOM uint32_t ui32DataLaneHSTrailTime : 8;
            __IOM uint32_t ui32DataLaneHSZeroTime : 8;
            __IOM uint32_t ui32DataLaneHSPreTime : 8;
        } ui32DataLaneHS_b;
    };

    union
    {
        __IOM uint32_t ui32ClockLaneHS;
        struct
        {
            __IOM uint32_t ui32ClockLaneHSCLKPostTime : 8;
            __IOM uint32_t ui32ClockLaneHSCLKPreTime : 8;
            __IOM uint32_t ui32ClockLaneHSTrailTime : 8;
            __IOM uint32_t ui32ClockLaneHSZeroTime : 8;
        } ui32ClockLaneHS_b;
    };

    union
    {
        __IOM uint32_t ui32Time;
        struct
        {
            __IOM uint32_t ui32HSTLPTime : 8;
            __IOM uint32_t ui32TexitTime : 8;
            __IOM uint32_t ui32ExtdCycleSel : 3;
            uint32_t : 13;
        } ui32Time_b;
    };

    union
    {
        __IOM uint32_t ui32PLLFBK;
        struct
        {
            __IOM uint32_t ui32PLLFBKInit : 9;
            __IOM uint32_t ui32PLLPosDiv : 2;
            __IOM uint32_t ui32PLLPreDiv : 4;
            uint32_t : 17;
        } ui32PLLFBK_b;
    };

    union
    {
        __IOM uint32_t ui32PLLFBKFRA;
        struct
        {
            __IOM uint32_t ui32PLLFBKFRA : 27;
            uint32_t : 5;
        } ui32PLLFBKFRA_b;
    };

    union
    {
        __IOM uint32_t ui32PLLSSCDelta;
        struct
        {
            __IOM uint32_t ui32PLLSSCDelta : 16;
            uint32_t : 16;
        } ui32PLLSSCDelta_b;
    };

    union
    {
        __IOM uint32_t ui32PLL;
        struct
        {
            uint32_t : 3;
            __IOM uint32_t ui32PLLSSCEn : 1;
            __IOM uint32_t ui32PLLSSCPRD : 10;
            uint32_t : 18;
        } ui32PLL_b;
    };

    //
    // Other parameters within this structure are required for configuring bitrate-related timing parameters;
    // since this specific parameter has limited impact, please ensure its value matches the actual bitrate.
    //
    uint32_t ui32BitRate;

#ifdef ATOMIQ11X_FPGA
    uint32_t ui32PllPreDiv;             //!< FPGA external DPHY PLL pre-divider.
    uint32_t ui32ExtdCycleSet;          //!< FPGA external DPHY extended cycle select.
    uint32_t ui32PllFbkInt;             //!< FPGA external DPHY PLL feedback integer.
    uint32_t ui32PllFbkFra;             //!< FPGA external DPHY PLL feedback fractional.
    uint32_t ui32DlaneHsPreTime;        //!< FPGA data-lane HS prepare time.
    uint32_t ui32DlaneHsZeroTime;       //!< FPGA data-lane HS zero time.
    uint32_t ui32DlaneHsTrailTime;      //!< FPGA data-lane HS trail time.
    uint32_t ui32ClaneHsPreTime;        //!< FPGA clock-lane HS prepare time.
    uint32_t ui32ClaneHsZeroTime;       //!< FPGA clock-lane HS zero time.
    uint32_t ui32ClaneHsTrailTime;      //!< FPGA clock-lane HS trail time.
    uint32_t ui32ClaneHsClkPreTime;     //!< FPGA clock-lane HS clock prepare time.
    uint32_t ui32ClaneHsClkPostTime;    //!< FPGA clock-lane HS clock post time.
#endif
}
am_hal_dsi_pll_time_config_t;

//
//! DSI Configuration Structure.
//
typedef struct
{
    //
    // Input parameters
    //

    uint8_t                         ui8LanesNum;        //!< Set the number of active lanes that are to be used for transmitting data. The value of this parameter can be 1 or 2.
    bool                            bContinuousClock;   //!< Set the Host Controller into continuous MIPI clock mode.
    uint8_t                         ui8TXGAP;           //!< Set the number of byte clock periods ('clk_byte' input) that the controller will wait after the clock lane has been put into LP mode before enabling the clock lane for HS mode. The minimum value for this port is 1.
    bool                            bAutoInsertEOTP;    //!< Enable the Host Controller to automatically insert an EoTp short packet when switching from HS to LP mode.

    bool                            bPPI16EN;           //!< Select either PPI_16 mode or 8 bit mode.

    bool                            bVidEnable;         //!< Set the controller to operate in video mode (true) or command mode (false).
    am_hal_dsi_video_pixel_format_e eVidPixelFormat;    //!< Set the DSI packet type of the pixels.
    am_hal_dsi_video_mode_e         eVidMode;           //!< Select DSI video mode that the host VID module should generate packets for.
    uint16_t                        ui16VidStartDelay;  //!< Set the number of cycles to delay the start of line.
    am_hal_dsi_video_timing_t       sVidTiming;         //!< Set the video timing parameters for video mode. This is only applicable when bVidOverride is set to true.
    am_hal_dsi_video_bllp_mode_e    eVidBllpMode;       //!< Optimize bllp periods to Low Power mode when possible
    bool                            bVidUseNullPktBllp; //!< Select type of blanking packet to be sent during bllp region
    uint8_t                         ui8VidVC;           //!< Set the virtual channel for video packets.

    //
    // DPHY related parameters
    //
    bool                            bSSCEn;             //!< Spread spectrum clock (SSC) mode enable
    am_hal_dsi_pll_time_config_t    sPLLTimeCfg;        //!< Parameters generated through gen_cdg_utility.
} am_hal_dsi_config_t;

// Processor to peripheral Direction Data Type
typedef enum
{
    //
    // short types
    //
    AM_HAL_DSI_DT_V_SYNC_START             =   0x01,
    AM_HAL_DSI_DT_V_SYNC_END               =   0x11,
    AM_HAL_DSI_DT_H_SYNC_START             =   0x21,
    AM_HAL_DSI_DT_H_SYNC_END               =   0x31,

    AM_HAL_DSI_DT_COLOR_MODE_OFF           =   0x02,
    AM_HAL_DSI_DT_COLOR_MODE_ON            =   0x12,
    AM_HAL_DSI_DT_SHUT_DOWN_PERIPH         =   0x22,
    AM_HAL_DSI_DT_TURN_ON_PERIPH           =   0x32,

    AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_0_PARAMETER         =   0x03,
    AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_1_PARAMETER         =   0x13,
    AM_HAL_DSI_DT_GENERIC_SHORT_WRITE_WITH_2_PARAMETERS        =   0x23,

    AM_HAL_DSI_DT_GENERIC_READ_WITH_0_PARAMETER                =   0x04,
    AM_HAL_DSI_DT_GENERIC_READ_WITH_1_PARAMETER                =   0x14,
    AM_HAL_DSI_DT_GENERIC_READ_WITH_2_PARAMETERS               =   0x24,

    AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_0_PARAMETER             =   0x05,
    AM_HAL_DSI_DT_DCS_SHORT_WRITE_WITH_1_PARAMETER             =   0x15,

    AM_HAL_DSI_DT_DCS_READ_WITH_0_PARAMETER                    =   0x06,

    AM_HAL_DSI_DT_SET_MAXIMUM_RETURN_PACKET_SIZE               =   0x37,

    AM_HAL_DSI_DT_EOTP                                         =   0x08,
    //
    // long types
    //
    AM_HAL_DSI_DT_NULL_PACKET                                  =   0x09,
    AM_HAL_DSI_DT_BLANKING_PACKET                              =   0x19,
    AM_HAL_DSI_DT_GENERIC_LONG_WRITE                           =   0x29,
    AM_HAL_DSI_DT_DCS_LONG_WRITE                               =   0x39,
// #### INTERNAL BEGIN ####
    AM_HAL_DSI_DT_LOOSELY_PACKED_PIXEL_STREAM_20_BIT_YCBCR_422_FORMAT      =   0x0C,
    AM_HAL_DSI_DT_PACKED_PIXEL_STREAM_24_BIT_YCBCR_422_FORMAT              =   0x1C,
    AM_HAL_DSI_DT_PACKED_PIXEL_STREAM_16_BIT_YCBCR_422_FORMAT              =   0x2C,
    AM_HAL_DSI_DT_PACKED_PIXEL_STREAM_30_BIT_RGB_101010_FORMAT             =   0x0D,
    AM_HAL_DSI_DT_PACKED_PIXEL_STREAM_36_BIT_RGB_121212_FORMAT             =   0x1D,
    AM_HAL_DSI_DT_PACKED_PIXEL_STREAM_12_BIT_YCBCR_420_FORMAT              =   0x3D,
// #### INTERNAL END ####
    AM_HAL_DSI_DT_PACKED_PIXEL_STREAM_16_BIT_RGB_565_FORMAT                =   0x0E,
    AM_HAL_DSI_DT_PACKED_PIXEL_STREAM_18_BIT_RGB_666_FORMAT                =   0x1E,
    AM_HAL_DSI_DT_LOOSELY_PACKED_PIXEL_STREAM_18_BIT_RGB_666_FORMAT        =   0x2E,
    AM_HAL_DSI_DT_PACKED_PIXEL_STREAM_24_BIT_RGB_888_FORMAT                =   0x3E,
}
am_hal_dsi_forward_data_type_e;

//
//! Peripheral to processor direction Data Type
//
typedef enum
{
    //
    // short types
    //
    AM_HAL_DSI_DT_ACKNOWLEDGE_AND_ERROR_REPORT                          = 0x02,
    AM_HAL_DSI_DT_END_OF_TRANSMISSION_PACKET                            = 0x08,
    AM_HAL_DSI_DT_GENERIC_SHORT_READ_RESPONSE_1_BYTE_RETURNED           = 0x11,
    AM_HAL_DSI_DT_GENERIC_SHORT_READ_RESPONSE_2_BYTES_RETURNED          = 0x12,
    AM_HAL_DSI_DT_DCS_SHORT_READ_RESPONSE_1_BYTE_RETURNED               = 0x21,
    AM_HAL_DSI_DT_DCS_SHORT_READ_RESPONSE_2_BYTES_RETURNED              = 0x22,
    //
    // long types
    //
    AM_HAL_DSI_DT_GENERIC_LONG_READ_RESPONSE                            = 0x1A,
    AM_HAL_DSI_DT_DCS_LONG_READ_RESPONSE                                = 0x1C,
}
am_hal_dsi_reversed_data_type_e;

typedef struct
{
    struct
    {
        uint32_t            *pui32TxPayload;
        union
        {
            __IOM uint32_t  ui32PktControl;
            struct
            {
                __IOM uint32_t  ui32PktWC : 16;              //!< Word count
                __IOM uint32_t  ui32PktVC : 2;               //!< Virtual channel
                __IOM uint32_t  ui32PktDT : 6;               //!< Data type
                __IOM uint32_t  ui32PktHSLPMode : 1;         //!< HS/LP select. 0 - LP mode. 1 - HS mode.
                __IOM uint32_t  ui32PktBTAAfterPacket : 1;   //!< Perform BTA immediately after packet is transmitted
                __IOM uint32_t  ui32PktBTAwithoutPacket : 1; //!< Perform BTA without sending a packet.
                uint32_t : 5;
            } ui32PktControl_b;
        };
    };
    struct
    {
        uint32_t            ui32PktRxPayload;
        union
        {
            __IOM uint32_t  ui32PktRxPktHeader;
            struct
            {
                __IOM uint32_t  ui32PktWC : 16;  //!< Word count
                __IOM uint32_t  ui32PktDT : 6;   //!< Data type
                __IOM uint32_t  ui32PktVC : 2;   //!< Virtual channel
                uint32_t : 8;
            } ui32PktRxPktHeader_b;
        };
    };

} am_hal_dsi_transfer_t;
//
//! DSI Control Request Types.
//
typedef enum
{
    AM_HAL_DSI_REQ_INPUT_CLK,
    AM_HAL_DSI_REQ_DSI_CLK_RESET_RELEASE,
    AM_HAL_DSI_REQ_DSI_CLK_RESET_HOLD,
    AM_HAL_DSI_REQ_ULPS_ENTRY,
    AM_HAL_DSI_REQ_ULPS_EXIT,
} am_hal_dsi_request_e;


typedef struct
{
    uint8_t                                 ui8EscClkDiv;   //!< Escape clock divider.

    uint8_t                                 ui8UlpsClkDiv;  //!< ULPS clock divider.

    CRM_DSIPHYREFCRM_DSIPHYREFCLKSEL_Enum   eRef;           //!< DSI PHY reference clock selection.

    uint8_t                                 ui8RefClkDiv;   //!< Reference clock divider.
}
am_hal_crm_dsi_clock_t;

//*****************************************************************************
//
// External function definitions
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Control the power state of the DSI module.
//!
//! @param pHandle      - The DSI state handle.
//! @param ePowerState  - Desired power state.
//! @param bRetainState - If true, hardware state is preserved.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_power_control(void *pHandle,
                                am_hal_sysctrl_power_state_e ePowerState,
                                bool bRetainState);

//*****************************************************************************
//
//! @brief Initialize the DSI module.
//!
//! @param ui32Module   - Instance of the DSI module.
//! @param ppHandle     - Pointer to store the populated handle.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_initialize(uint32_t ui32Module, void **ppHandle);

//*****************************************************************************
//
//! @brief Uninitialize the DSI module.
//!
//! @param pHandle      - The DSI state handle.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_uninitialize(void *pHandle);

//*****************************************************************************
//
//! @brief Configure the DSI module.
//!
//! Programs host/PHY registers, then releases DSI clock resets
//! (`AM_HAL_DSI_REQ_DSI_CLK_RESET_RELEASE`). Call after
//! `am_hal_dsi_power_control(WAKE)`; a separate reset-release is not required.
//!
//! @param pHandle      - The DSI state handle.
//! @param pConfig      - Pointer to the configuration structure.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_config(void *pHandle, const void *pConfig);

//*****************************************************************************
//
//! @brief Initiate a non-blocking data transfer.
//!
//! @param pHandle      - The DISP state handle.
//! @param pTransfer    - Pointer to the transfer descriptor.
//! @param pfnCallback  - Optional completion callback.
//! @param pCallbackCtxt- Context pointer for the callback.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t
am_hal_dsi_nonblocking_transfer(void *pHandle,
                               am_hal_dsi_transfer_t *pTransfer,
                               am_hal_dsi_callback_t pfnCallback,
                               void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Perform a blocking data transfer.
//!
//! @param pHandle      - The DISP state handle.
//! @param pTransfer    - Pointer to the transfer descriptor.
//! @param ui32Timeoutus- Timeout in microseconds.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t
am_hal_dsi_blocking_transfer(void *pHandle,
                            am_hal_dsi_transfer_t *pTransfer,
                            uint32_t ui32Timeoutus);

//*****************************************************************************
//
//! @brief Perform a specific control operation on the DSI.
//!
//! @param pHandle      - The DSI state handle.
//! @param eRequest     - The control request type.
//! @param pConfig      - Pointer to request-specific configuration data.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_control(void *pHandle, am_hal_dsi_request_e eRequest, const void *pConfig);

//*****************************************************************************
//
//! @brief Enable specific DSI interrupts.
//!
//! @param pHandle       - The DSI state handle.
//! @param pui32IntMask  - Pointer to interrupt mask (IRQ mask and mask2).
//!
//! @return status       - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_interrupt_enable(void *pHandle, uint32_t *pui32IntMask);

//*****************************************************************************
//
//! @brief Disable specific DSI interrupts.
//!
//! @param pHandle       - The DSI state handle.
//! @param pui32IntMask  - Pointer to interrupt mask (IRQ mask and mask2).
//!
//! @return status       - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_interrupt_disable(void *pHandle, uint32_t *pui32IntMask);

//*****************************************************************************
//
//! @brief Retrieve the current DSI interrupt status.
//!
//! @param pHandle      - The DSI state handle.
//! @param pui32Status  - Pointer to store the status.
//! @param bEnabledOnly - If true, only returns enabled interrupts.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_interrupt_status_get(void *pHandle, uint32_t *pui32Status, bool bEnabledOnly);

//*****************************************************************************
//
//! @brief Clear specific DSI interrupts.
//!
//! @param pHandle      - The DSI state handle.
//! @param ui32IntMask  - Mask of interrupts to clear.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_interrupt_clear(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Process DSI interrupts.
//!
//! @param pHandle        - The DSI state handle.
//! @param pui32IntStatus - Pointer to the interrupt status mask.
//!
//! @return status        - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_interrupt_service(void *pHandle, uint32_t *pui32IntStatus);

//*****************************************************************************
//
//! @brief Get the current hardware status of the DSI.
//!
//! @param pHandle      - The DSI state handle.
//! @param pui32Status  - Pointer to store the status register value.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dsi_status_get(void *pHandle, uint32_t *pui32Status);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_DSI_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
