//*****************************************************************************
//
//! @file am_hal_xspimc_ll.h
//!
//! @brief Low Level functions for interfacing with the xSPI.
//!
//! @addtogroup ll_xspi_at110 xSPI - Multi-bit SPI
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
#ifndef AM_HAL_XSPIMC_LL_H
#define AM_HAL_XSPIMC_LL_H

#include "atomiq110.h"

#ifdef __cplusplus
extern "C"
{
#endif
//temp
#ifndef __fallthrough
#define __fallthrough        __attribute__((fallthrough))
#endif
//#define TFM_BUILD_SECURE
typedef struct
{
    // AIP SPI Clock Configuration (0x70)
    union
    {
        volatile uint32_t AIPSPICLKCFG;
        struct
        {
            volatile uint32_t AIPSAMP         : 8;              // [31:24]: Sample point
            uint32_t                          : 6;
            volatile uint32_t AIPCLOCKPOLARITY: 1;              // [17]: Clock phase
            volatile uint32_t AIPTRANSMITON   : 1;              // [16]: Transmit data on
            volatile uint32_t AIPMINCSLOW     : 8;              // [15:8]: Min. CS Low
            volatile uint32_t AIPCLOCKPERIOD  : 8;              // [7:0] Clock period
        } AIPSPICLKCFG_b;
    };
    // AIP Minimum CS High (0x74)
    union
    {
        volatile uint32_t AIPMINCSHIGH;
        struct
        {
            volatile uint32_t AIPMAXCSLOW     : 24;             // [31:8]: Max CS Low
            volatile uint32_t AIPMINCSHIGH    : 8;              // [7:0]: Min CS High
        } AIPMINCSHIGH_b;
    };
    // AIP Hold time (0x78)
    union
    {
        volatile uint32_t AIPHOLDTIME;
        struct
        {
            uint32_t                          : 24;
            volatile uint32_t AIPHOLDTIME     : 8;              // [7:0]: AIP Hold time
        } AIPHOLDTIME_b;
    };
    volatile const uint32_t RESERVED5;
    // AIP Control (0x80)
    union
    {
        volatile uint32_t AIPCTRL;
        struct
        {
            uint32_t                          : 19;
            volatile uint32_t WOPT            : 1;               // [12]: set to 1 write operations on through the AIP interface to this window may be merged
            volatile uint32_t CRCCALC         : 1;               // [11]: start AIP configuration CRC calculation
            volatile uint32_t PFENABLE        : 1;               // [10]: Prefetch Logic Enabled
            volatile uint32_t UPDATE          : 1;               // [9]: Update
            volatile uint32_t SUSRES          : 1;               // [8]: Erase/Write Suspend Resume Enable
            uint32_t                          : 6;
            volatile uint32_t AIPBUSY         : 1;               // [1]: AIP operation state
            volatile uint32_t AIPMODEENABLE   : 1;               // [0]: XIP Enable
        } AIPCTRL_b;
    };
    // AIP Access Boundary (0x84)
    union
    {
        volatile uint32_t AIPACCB;
        struct
        {
            uint32_t                            : 27;
            volatile uint32_t AIPACCESSBOUNDARY : 5;
        } AIPACCB_b;
    };
    // AIP Win Size (0x88)
    volatile uint32_t       AIPSIZE;
    // AIP Window Address (0x8c)
    volatile uint32_t       AIPWINADDR;
    // New AIP Command (0x90)
    union
    {
        volatile uint32_t AIPCMD;
        struct
        {
            volatile uint32_t AIPWCMD         : 16;              // [31:16]: W Command
            volatile uint32_t AIPRCMD         : 16;              // [15:0]: R Command
        } AIPCMD_b;
    };
    // New AIP Command Attributes (0x94)
    union
    {
        volatile uint32_t AIPCMDATTR;
        struct
        {
            uint32_t                          : 15;
            volatile uint32_t AIPCMDSKIP      : 1;               // [16]: Command skip
            volatile uint32_t AIPCMDHRC       : 1;               // [15]: HyperRAM command
            volatile uint32_t AIPWRITEEN      : 1;               // [14]: AIP write support
            volatile uint32_t AIPCMDSD        : 1;               // [13]: Stacked-die mode
            volatile uint32_t AIPCMDDDR       : 1;               // [12]: Dtr mode
            volatile uint32_t AIPCMDMODE      : 4;               // [11:8]: Output mode
            uint32_t                          : 4;
            volatile uint32_t AIPCMDLEN       : 4;               // [3:0]: Duration
        } AIPCMDATTR_b;
    };
    // New AIP Die Select Command (0x98)
    union
    {
        volatile uint32_t AIPDIECMD;
        struct
        {
            uint32_t                          : 11;
            volatile uint32_t ADDRBITSINDIE   : 5;                // [20:16]: Number of address bits on a single die
            volatile uint32_t DIESELECTCMD    : 16;               // [15: 0]: Command
        } AIPDIECMD_b;
    };
    // New AIP Die Select Attributes (0x9c)
    union
    {
        volatile uint32_t AIPDIEATTR;
        struct
        {
            uint32_t                          : 14;
            volatile uint32_t AIPDIECMDAWIDTH : 2;               // [17:16]: Address width
            uint32_t                          : 1;
            volatile uint32_t AIPDIECMDEN     : 1;               // [14]: Enable
            volatile uint32_t AIPDIECMDSD     : 1;               // [13]: Stacked-die mode
            volatile uint32_t AIPDIECMDDDR    : 1;               // [12]: Dtr mode
            volatile uint32_t AIPDIECMDMODE   : 4;               // [11:8]: Output mode
            uint32_t                          : 4;
            volatile uint32_t AIPDIECMDLEN    : 4;               // [3:0] Command duration
        } AIPDIEATTR_b;
    };
    // New AIP Base Address (0xA0)
    volatile uint32_t       AIPADDRBASE;
    // New AIP Address Attributes (0xA4)
    union
    {
        volatile uint32_t AIPADDRATTR;
        struct
        {
            uint32_t                          : 1;
            volatile uint32_t AIPADDRTRANS    : 3;               // [30:28]: Address translation
            uint32_t                          : 2;
            volatile uint32_t AIPADDRSHIFT    : 2;               // [25:24]: Address shift
            uint32_t                          : 3;
            volatile uint32_t AIPADDRMASK     : 3;               // [20:18]: Address mask
            volatile uint32_t AIPADDRWIDTH    : 2;               // [17:16]: Address width
            uint32_t                          : 2;
            volatile uint32_t AIPADDRSD       : 1;               // [13]: Stacked-die mode
            volatile uint32_t AIPADDRDTR      : 1;               // [12]: Dtr mode
            volatile uint32_t AIPADDRMODE     : 4;               // [11:8]: Output mode
            uint32_t                          : 8;
        } AIPADDRATTR_b;
    };
    // New AIP Chip Select (0xA8)
    volatile uint32_t       AIPCS;
    // New AIP Die Page State (0xAC)
    union
    {
        volatile uint32_t AIPDIEPAGESTATE;
        struct
        {
            volatile uint32_t AIPDIESELECTED  : 1;
            volatile uint32_t AIPSELECTEDDIE  : 7;
            volatile uint32_t AIPPAGESELECTED : 1;
            volatile uint32_t AIPSELECTEDPAGE : 23;
        } AIPDIEPAGESTATE_b;
    };
    // New AIP Dummy Attributes (0xB0)
    union
    {
        volatile uint32_t AIPDUMMYATTR;
        struct
        {
            uint32_t                          : 6;
            volatile uint32_t AIPWLATENCYDYN  : 1;              // [25]: Dynamic write latency
            volatile uint32_t AIPWLATENCYSKIP : 1;              // [24]: Skip write latency
            uint32_t                          : 3;
            volatile uint32_t AIPWLATENCYLEN  : 5;              // [20:16]: Write latency
            uint32_t                          : 7;
            volatile uint32_t AIPRDUMMYSKIP   : 1;              // [8]: Dummy skip
            uint32_t                          : 3;
            volatile uint32_t AIPRDUMMYLEN    : 5;              // [4:0]: Duration
        } AIPDUMMYATTR_b;
    };
    // New AIP Suspend/Resume Command (0xB4)
    union
    {
        volatile uint32_t AIPSUSRESCMD;
        struct
        {
            volatile uint32_t AIPRESCMD       : 16;                 // [31:16]
            volatile uint32_t AIPSUSCMD       : 16;                 // [15:0]
        } AIPSUSRESCMD_b;
    };
    // New AIP Suspend/Resume Command Attributes (0xB8)
    union
    {
        volatile uint32_t AIPSUSRESATTR;
        struct
        {
            uint32_t                          : 18;
            volatile uint32_t SRCMDSD         : 1;               // [13]: Stacked-die mode
            volatile uint32_t SRCMDDDR        : 1;               // [12]: Dtr mode
            volatile uint32_t SRCMDMODE       : 4;               // [11:8]: Output mode
            uint32_t                          : 4;
            volatile uint32_t SRCMDLEN        : 4;               // [3:0]: Duration
        } AIPSUSRESATTR_b;
    };
    // New AIP Suspend Time (0xBC)
    volatile uint32_t       AIPSUSTIME; // in bus clock cycles
    // New AIP Data Attributes (0xC0)
    union
    {
        volatile uint32_t AIPDATATTR;
        struct
        {
            uint32_t                          : 15;
            volatile uint32_t AIPDATAKCH      : 1;              // [16]: Keep clock high @ de-select
            uint32_t                          : 8;
            volatile uint32_t AIPDATAFB       : 1;              // [7]: Flip bytes
            volatile uint32_t AIPDATADQS      : 1;              // [6]: DQS based sampling
            volatile uint32_t AIPDATASD       : 1;              // [5]: Stacked-die mode
            volatile uint32_t AIPDATADDR      : 1;              // [4]: DDR mode
            volatile uint32_t AIPDATAMODE     : 4;              // [3:0]: Mode
        } AIPDATATTR_b;
    };
    // New AIP Resume Time (0xC4)
    volatile uint32_t       AIPRESTIME; // in bus clock cycles
    // New AIP Suspend to Resume Time (0xC8)
    volatile uint32_t       AIPSUSRESTIME;
    // New AIP Resume to Suspend Time (0xCC)
    volatile uint32_t       AIPRESSUSTIME;
    // New AIP Mode Bits (0xD0)
    volatile uint32_t       AIPMODEBITS;
    // New AIP Mode Bits Attributes (0xD4)
    union
    {
        volatile uint32_t AIPMODEBITSATTR;
        struct
        {
            uint32_t                           : 15;
            volatile uint32_t AIPMODEBITSSKIP  : 1;              // [16]: Mode skip
            uint32_t                           : 2;
            volatile uint32_t AIPMODEBITSSD    : 1;              // [13]: Stacked-die mode
            volatile uint32_t AIPMODEBITSDDR   : 1;              // [12]: DDR mode
            volatile uint32_t AIPMODEBITSMODE  : 4;              // [11:8]: Output mode
            uint32_t                           : 3;
            volatile uint32_t AIPMODEBITSLEN   : 5;              // [4:0]: Duration
        } AIPMODEBITSATTR_b;
    };
    // New AIP Page Read command attributes (0xD8)
    union
    {
        volatile uint32_t AIPPRCMDATTR;
        struct
        {
            uint32_t                          : 13;
            volatile uint32_t ADDRESSWIDTH    : 2;              // [18:17]: Address width
            uint32_t                          : 2;
            volatile uint32_t PAGEENABLE      : 1;               // [14]: Page Enable
            volatile uint32_t PRCMDSD         : 1;               // [13]: Stacked-die mode
            volatile uint32_t PRCMDDDR        : 1;               // [12]: Dtr mode
            volatile uint32_t PRCMDMODE       : 4;               // [11:8]: Output mode
            uint32_t                          : 4;
            volatile uint32_t PRCMDLEN        : 4;               // [3:0]: Duration
        } AIP_PRCMD_ATTR_b;
    };
    // New AIP Read Page Data Time (0xDC)
    volatile uint32_t       AIPPAGETIME; // Execution time of a Page Read command in bus clock cycles
} AIP_Type;

typedef struct
{
    // Input delay 1
    union
    {
        volatile uint32_t DELAY1;
        struct
        {
            volatile uint32_t DQS1DELAY      : 8;
            volatile uint32_t DQS2DELAY      : 8;
            uint32_t                         : 16;
        } DELAY1_b;
    };
    // Input delay 2
    union
    {
        volatile uint32_t DELAY2;
        struct
        {
            volatile uint32_t DQ0DELAY       : 8;
            volatile uint32_t DQ1DELAY       : 8;
            volatile uint32_t DQ2DELAY       : 8;
            volatile uint32_t DQ3DELAY       : 8;
        } DELAY2_b;
    };
    // Input delay 3
    union
    {
        volatile uint32_t DELAY3;
        struct
        {
            volatile uint32_t DQ4DELAY       : 8;
            volatile uint32_t DQ5DELAY       : 8;
            volatile uint32_t DQ6DELAY       : 8;
            volatile uint32_t DQ7DELAY       : 8;
        } DELAY3_b;
    };
    // Input delay 4
    union
    {
        volatile uint32_t DELAY4;
        struct
        {
            volatile uint32_t DQ8DELAY       : 8;
            volatile uint32_t DQ9DELAY       : 8;
            volatile uint32_t DQ10DELAY      : 8;
            volatile uint32_t DQ11DELAY      : 8;
        } DELAY4_b;
    };
    // Input delay 5
    union
    {
        volatile uint32_t DELAY5;
        struct
        {
            volatile uint32_t DQ12DELAY      : 8;
            volatile uint32_t DQ13DELAY      : 8;
            volatile uint32_t DQ14DELAY      : 8;
            volatile uint32_t DQ15DELAY      : 8;
        } DELAY5_b;
    };
} XSPIMC_DELAY_Type;

typedef struct
{
    // Low-power SPI Clock Configuration
    union
    {
        volatile uint32_t CSLPCLK;
        struct
        {
            volatile uint32_t CLOCKPERIOD    : 8;
            volatile uint32_t MINCSLOW       : 8;
            volatile uint32_t TRANSMITDATAON : 1;
            volatile uint32_t CLOCKPOLARITY  : 1;
            uint32_t                         : 14;
        } CSLPCLK_b;
    };
    // Low-power Minimum CS High
    union
    {
        volatile uint32_t CSLPCS;
        struct
        {
            volatile uint32_t LPENTRYMINCSH  : 8;
            volatile uint32_t LPEXITMINCSH   : 24;
        } CSLPCS_b;
    };
    // Low-power entry command 1
    volatile uint32_t CSLPENTRYCMD1;
    // Low-power entry command 2
    volatile uint32_t CSLPENTRYCMD2;
    // Low-power exit command 1
    volatile uint32_t CSLPEXITCMD1;
    // Low-power exit command 2
    volatile uint32_t CSLPEXITCMD2;
    // Low-power entry/exit command attributes
    union
    {
        volatile uint32_t CSLPATTR;
        struct
        {
            volatile uint32_t DURATION       : 4;
            uint32_t                         : 4;
            volatile uint32_t MODE           : 4;
            volatile uint32_t DDRMODE        : 1;
            volatile uint32_t STACKEDDIE     : 1;
            uint32_t                         : 2;
            volatile uint32_t USELPENTRY     : 1;
            volatile uint32_t USELPEXIT      : 1;
            uint32_t                         : 14;
        } CSLPATTR_b;
    };
} CSLP_Type;

typedef struct
{
    // Implemented Features (0x180)
    union
    {
        volatile uint32_t AIPIMPFEAT;
        struct
        {
            uint32_t                          : 6;
            volatile uint32_t ENCIMPL         : 1;             // [25]: If set to 1, memory encryption is implemented
            volatile uint32_t LPCSIMPL        : 1;             // [24]: If set to 1, device auto Low power is implemented
            uint32_t                          : 7;
            volatile uint32_t ASILIMPL        : 1;             // [16]: If set to 1, ASIL support is implemented
            uint32_t                          : 3;
            volatile uint32_t SRAMIMPL        : 1;             // [12]: If set to 1, SRAM support is implemented
            uint32_t                          : 3;
            volatile uint32_t ACFGIMPL        : 1;             // [8]: If set to 1, the auto configuration module is implemented
            uint32_t                          : 3;
            volatile uint32_t DMAIMPL         : 1;             // [4]: If set to 1, the DMA engine is implemented
            uint32_t                          : 3;
            volatile uint32_t AIPIMPL         : 1;             // [0]: If set to 1, AIP interface is implemented
        } AIPIMPFEAT_b;
    };
    // Implemented number of CS lines (0x184)
    volatile uint32_t       AIPIMPCSW;
    // Implemented number of GPIO lines (0x188)
    volatile uint32_t       AIPIMPGPIOW;
    // Implemented number of data lines (0x18C)
    volatile uint32_t       AIPIMPDQWIDTH;
    // Implemented Clocking option (0x190)
    volatile uint32_t       IMPCDC;
    // Implemented AIP type (0x194)
    volatile uint32_t       AIPIMPTYPE;
    // Implemented AIP address width (0x198)
    volatile uint32_t       AIPIMPAW;
    // Implemented AIP ID width (0x19C)
    volatile uint32_t       AIPIMPIDW;
    // Implemented custom command FIFO (0x1A0)
    volatile uint32_t       CFGFIFOAW;
    volatile const uint32_t RESERVED8;
    // Implemented write FIFO depth (0x1A8)
    volatile uint32_t       IMPWRFIFOAW;
    volatile const uint32_t RESERVED9;
    // Implemented read FIFO depth (0x1B0)
    volatile uint32_t       IMPRDFIFOAW;
    volatile const uint32_t RESERVED10;
    // Implemented number of input delay taps (0x1B8)
    volatile uint32_t       IMPDELAY;
    // Implemented DMA address width (0x1BC)
    volatile uint32_t       IMPDMAAW;
    // Implemented number of available AIP (0x1C0)
    volatile uint32_t       ADDRDECN;
    // ASIL control register (0x1C4)
    union
    {
        volatile uint32_t ASILCTRL;
        struct
        {
            uint32_t                          : 3;
            volatile uint32_t REQWRSEQERR     : 1;             // [28]
            volatile uint32_t REQRDSEQERR     : 1;             // [27]
            volatile uint32_t PFTCRDSEQERR    : 1;             // [26]
            volatile uint32_t AIPRDSEQERR     : 1;             // [25]
            volatile uint32_t AIPRDCRCERR     : 1;             // [24]
            volatile uint32_t DMARDSEQERR     : 1;             // [23]
            volatile uint32_t DMARDCRCERR     : 1;             // [22]
            volatile uint32_t PHYSEQERR       : 1;             // [21]
            volatile uint32_t PHYWDCRCERR     : 1;             // [20]
            volatile uint32_t PHYCMDCRCERR    : 1;             // [19]
            volatile uint32_t PHYADRCRCERR    : 1;             // [18]
            volatile uint32_t PHYLENCRCERR    : 1;             // [17]
            volatile uint32_t PHYCONSTCRCERR  : 1;             // [16]
            uint32_t                          : 15;
            volatile uint32_t CRCCALC         : 1;
        } ASILCTRL_b;
    };
    // Power-up test sequence status (0x1C8)
    union
    {
        volatile uint32_t CRCTSTSTS;
        struct
        {
            uint32_t                          : 30;
            volatile uint32_t SEQDONE         : 1;
            volatile uint32_t CRCDONE         : 1;
        } CRCTSTSTS_b;
    };
    // Power-up test sequence error (0x1CC)
    union
    {
        volatile uint32_t CRCTSTERROR;
        struct
        {
            uint32_t                          : 30;
            volatile uint32_t SEQERROR        : 1;
            volatile uint32_t CRCERROR        : 1;
        } CRCTSTERROR_b;
    };
} CAPS_TYPE;

typedef XSPIMC0_Type XSPIMC_Type;

//*****************************************************************************
//
//! @brief xSPI Global CMSIS-Style macros
//! @defgroup ll_xspi_macro_at110
//! @ingroup ll_xspi_at110
//! @{
//
//*****************************************************************************
// #### INTERNAL BEGIN ####
#ifdef FPGA_FREQ_RATIO
//Maximum delay time in us for transferring a word with margin
#define AM_HAL_XSPIMC_MAX_DELAY_CNT (200 * FPGA_FREQ_RATIO)
#else
// #### INTERNAL END ####
//Maximum delay time in us for transferring a word with margin
#define AM_HAL_XSPIMC_MAX_DELAY_CNT (200)
// #### INTERNAL BEGIN ####
#endif
// #### INTERNAL END ####

#define AM_HAL_XSPIMC_MAX_DEVICES              2
#define AM_HAL_XSPIMC_MAX_AIP_WINDOWS          4
#define AM_HAL_XSPIMC_AIP_WINDOWS_PER_DEVICE   2

#if defined(TFM_BUILD_SECURE)

#define XSPIMC0_APERTURE_START_ADDR            0x90000000
#define XSPIMC1_APERTURE_START_ADDR            0xB0000000
#define XSPIMC2_APERTURE_START_ADDR            0xD0000000
#define XSPIMC0_APERTURE_END_ADDR              0xA0000000
#define XSPIMC1_APERTURE_END_ADDR              0xC0000000
#define XSPIMC2_APERTURE_END_ADDR              0xE0000000

#else

#define XSPIMC0_APERTURE_START_ADDR            0x80000000
#define XSPIMC1_APERTURE_START_ADDR            0xA0000000
#define XSPIMC2_APERTURE_START_ADDR            0xC0000000
#define XSPIMC0_APERTURE_END_ADDR              0x90000000
#define XSPIMC1_APERTURE_END_ADDR              0xB0000000
#define XSPIMC2_APERTURE_END_ADDR              0xD0000000

#endif

//*****************************************************************************
//
//! @name xSPI Register Bitfield Definitions (CMSIS-style)
//! @brief Position and mask definitions for register fields in `XSPIMC_Type`,
//! `AIP_Type`, and `CAPS_TYPE`.
//!
//! Naming convention:
//! - `XSPIMC_<REG>_<FIELD>_Pos`: bit position (LSB) of field
//! - `XSPIMC_<REG>_<FIELD>_Msk`: field mask
//! @{
//
//*****************************************************************************

// ----------------------------
// AIP_Type register fields
// ----------------------------

// AIPSPICLKCFG (0x70)
#define XSPIMC0_AIPSPICLKCFG_AIPSAMP_Pos        24u
#define XSPIMC0_AIPSPICLKCFG_AIPSAMP_Msk        (0xFFu << XSPIMC0_AIPSPICLKCFG_AIPSAMP_Pos)
#define XSPIMC0_AIPSPICLKCFG_AIPCLOCKPOLARITY_Pos 17u
#define XSPIMC0_AIPSPICLKCFG_AIPCLOCKPOLARITY_Msk (0x1u << XSPIMC0_AIPSPICLKCFG_AIPCLOCKPOLARITY_Pos)
#define XSPIMC0_AIPSPICLKCFG_AIPTRANSMITON_Pos  16u
#define XSPIMC0_AIPSPICLKCFG_AIPTRANSMITON_Msk  (0x1u << XSPIMC0_AIPSPICLKCFG_AIPTRANSMITON_Pos)
#define XSPIMC0_AIPSPICLKCFG_AIPMINCSLOW_Pos    8u
#define XSPIMC0_AIPSPICLKCFG_AIPMINCSLOW_Msk    (0xFFu << XSPIMC0_AIPSPICLKCFG_AIPMINCSLOW_Pos)
#define XSPIMC0_AIPSPICLKCFG_AIPCLOCKPERIOD_Pos 0u
#define XSPIMC0_AIPSPICLKCFG_AIPCLOCKPERIOD_Msk (0xFFu << XSPIMC0_AIPSPICLKCFG_AIPCLOCKPERIOD_Pos)

// AIPMINCSHIGH (0x74)
#define XSPIMC0_AIPMINCSHIGH_AIPMAXCSLOW_Pos    8u
#define XSPIMC0_AIPMINCSHIGH_AIPMAXCSLOW_Msk    (0xFFu << XSPIMC0_AIPMINCSHIGH_AIPMAXCSLOW_Pos)
#define XSPIMC0_AIPMINCSHIGH_AIPMINCSHIGH_Pos   0u
#define XSPIMC0_AIPMINCSHIGH_AIPMINCSHIGH_Msk   (0xFFu << XSPIMC0_AIPMINCSHIGH_AIPMINCSHIGH_Pos)

// AIPHOLDTIME (0x78)
#define XSPIMC0_AIPHOLDTIME_AIPHOLDTIME_Pos     0u
#define XSPIMC0_AIPHOLDTIME_AIPHOLDTIME_Msk     (0xFFu << XSPIMC0_AIPHOLDTIME_AIPHOLDTIME_Pos)

// AIPCTRL (0x80)
#define XSPIMC0_AIPCTRL_WOPT_Pos                12u
#define XSPIMC0_AIPCTRL_WOPT_Msk                (0x1u << XSPIMC0_AIPCTRL_WOPT_Pos)
#define XSPIMC0_AIPCTRL_CRCCALC_Pos             11u
#define XSPIMC0_AIPCTRL_CRCCALC_Msk             (0x1u << XSPIMC0_AIPCTRL_CRCCALC_Pos)
#define XSPIMC0_AIPCTRL_PFENABLE_Pos            10u
#define XSPIMC0_AIPCTRL_PFENABLE_Msk            (0x1u << XSPIMC0_AIPCTRL_PFENABLE_Pos)
#define XSPIMC0_AIPCTRL_UPDATE_Pos              9u
#define XSPIMC0_AIPCTRL_UPDATE_Msk              (0x1u << XSPIMC0_AIPCTRL_UPDATE_Pos)
#define XSPIMC0_AIPCTRL_SUSRES_Pos              8u
#define XSPIMC0_AIPCTRL_SUSRES_Msk              (0x1u << XSPIMC0_AIPCTRL_SUSRES_Pos)
#define XSPIMC0_AIPCTRL_AIPBUSY_Pos             1u
#define XSPIMC0_AIPCTRL_AIPBUSY_Msk             (0x1u << XSPIMC0_AIPCTRL_AIPBUSY_Pos)
#define XSPIMC0_AIPCTRL_AIPMODEENABLE_Pos       0u
#define XSPIMC0_AIPCTRL_AIPMODEENABLE_Msk       (0x1u << XSPIMC0_AIPCTRL_AIPMODEENABLE_Pos)

// AIPACCB (0x84)
#define XSPIMC0_AIPACCB_AIPACCESSBOUNDARY_Pos   0u
#define XSPIMC0_AIPACCB_AIPACCESSBOUNDARY_Msk   (0x1Fu << XSPIMC0_AIPACCB_AIPACCESSBOUNDARY_Pos)

// AIPCMD (0x90)
#define XSPIMC0_AIPCMD_AIPWCMD_Pos              16u
#define XSPIMC0_AIPCMD_AIPWCMD_Msk              (0xFFFFu << XSPIMC0_AIPCMD_AIPWCMD_Pos)
#define XSPIMC0_AIPCMD_AIPRCMD_Pos              0u
#define XSPIMC0_AIPCMD_AIPRCMD_Msk              (0xFFFFu << XSPIMC0_AIPCMD_AIPRCMD_Pos)

// AIPCMDATTR (0x94)
#define XSPIMC0_AIPCMDATTR_AIPCMDSKIP_Pos       16u
#define XSPIMC0_AIPCMDATTR_AIPCMDSKIP_Msk       (0x1u << XSPIMC0_AIPCMDATTR_AIPCMDSKIP_Pos)
#define XSPIMC0_AIPCMDATTR_AIPCMDHRC_Pos        15u
#define XSPIMC0_AIPCMDATTR_AIPCMDHRC_Msk        (0x1u << XSPIMC0_AIPCMDATTR_AIPCMDHRC_Pos)
#define XSPIMC0_AIPCMDATTR_AIPWRITEEN_Pos       14u
#define XSPIMC0_AIPCMDATTR_AIPWRITEEN_Msk       (0x1u << XSPIMC0_AIPCMDATTR_AIPWRITEEN_Pos)
#define XSPIMC0_AIPCMDATTR_AIPCMDSD_Pos         13u
#define XSPIMC0_AIPCMDATTR_AIPCMDSD_Msk         (0x1u << XSPIMC0_AIPCMDATTR_AIPCMDSD_Pos)
#define XSPIMC0_AIPCMDATTR_AIPCMDDDR_Pos        12u
#define XSPIMC0_AIPCMDATTR_AIPCMDDDR_Msk        (0x1u << XSPIMC0_AIPCMDATTR_AIPCMDDDR_Pos)
#define XSPIMC0_AIPCMDATTR_AIPCMDMODE_Pos       8u
#define XSPIMC0_AIPCMDATTR_AIPCMDMODE_Msk       (0xFu << XSPIMC0_AIPCMDATTR_AIPCMDMODE_Pos)
#define XSPIMC0_AIPCMDATTR_AIPCMDLEN_Pos        0u
#define XSPIMC0_AIPCMDATTR_AIPCMDLEN_Msk        (0xFu << XSPIMC0_AIPCMDATTR_AIPCMDLEN_Pos)

// AIPDIECMD (0x98)
#define XSPIMC0_AIPDIECMD_ADDRBITSINDIE_Pos     16u
#define XSPIMC0_AIPDIECMD_ADDRBITSINDIE_Msk     (0x1Fu << XSPIMC0_AIPDIECMD_ADDRBITSINDIE_Pos)
#define XSPIMC0_AIPDIECMD_DIESELECTCMD_Pos      0u
#define XSPIMC0_AIPDIECMD_DIESELECTCMD_Msk      (0xFFFFu << XSPIMC0_AIPDIECMD_DIESELECTCMD_Pos)

// AIPDIEATTR (0x9C)
#define XSPIMC0_AIPDIEATTR_AIPDIECMDAWIDTH_Pos  16u
#define XSPIMC0_AIPDIEATTR_AIPDIECMDAWIDTH_Msk  (0x3u << XSPIMC0_AIPDIEATTR_AIPDIECMDAWIDTH_Pos)
#define XSPIMC0_AIPDIEATTR_AIPDIECMDEN_Pos      14u
#define XSPIMC0_AIPDIEATTR_AIPDIECMDEN_Msk      (0x1u << XSPIMC0_AIPDIEATTR_AIPDIECMDEN_Pos)
#define XSPIMC0_AIPDIEATTR_AIPDIECMDSD_Pos      13u
#define XSPIMC0_AIPDIEATTR_AIPDIECMDSD_Msk      (0x1u << XSPIMC0_AIPDIEATTR_AIPDIECMDSD_Pos)
#define XSPIMC0_AIPDIEATTR_AIPDIECMDDDR_Pos     12u
#define XSPIMC0_AIPDIEATTR_AIPDIECMDDDR_Msk     (0x1u << XSPIMC0_AIPDIEATTR_AIPDIECMDDDR_Pos)
#define XSPIMC0_AIPDIEATTR_AIPDIECMDMODE_Pos    8u
#define XSPIMC0_AIPDIEATTR_AIPDIECMDMODE_Msk    (0xFu << XSPIMC0_AIPDIEATTR_AIPDIECMDMODE_Pos)
#define XSPIMC0_AIPDIEATTR_AIPDIECMDLEN_Pos     0u
#define XSPIMC0_AIPDIEATTR_AIPDIECMDLEN_Msk     (0xFu << XSPIMC0_AIPDIEATTR_AIPDIECMDLEN_Pos)

// AIPADDRATTR (0xA4)
#define XSPIMC0_AIPADDRATTR_AIPADDRTRANS_Pos    28u
#define XSPIMC0_AIPADDRATTR_AIPADDRTRANS_Msk    (0x7u << XSPIMC0_AIPADDRATTR_AIPADDRTRANS_Pos)
#define XSPIMC0_AIPADDRATTR_AIPADDRSHIFT_Pos    24u
#define XSPIMC0_AIPADDRATTR_AIPADDRSHIFT_Msk    (0x3u << XSPIMC0_AIPADDRATTR_AIPADDRSHIFT_Pos)
#define XSPIMC0_AIPADDRATTR_AIPADDRMASK_Pos     18u
#define XSPIMC0_AIPADDRATTR_AIPADDRMASK_Msk     (0x7u << XSPIMC0_AIPADDRATTR_AIPADDRMASK_Pos)
#define XSPIMC0_AIPADDRATTR_AIPADDRWIDTH_Pos    16u
#define XSPIMC0_AIPADDRATTR_AIPADDRWIDTH_Msk    (0x3u << XSPIMC0_AIPADDRATTR_AIPADDRWIDTH_Pos)
#define XSPIMC0_AIPADDRATTR_AIPADDRSD_Pos       13u
#define XSPIMC0_AIPADDRATTR_AIPADDRSD_Msk       (0x1u << XSPIMC0_AIPADDRATTR_AIPADDRSD_Pos)
#define XSPIMC0_AIPADDRATTR_AIPADDRDTR_Pos      12u
#define XSPIMC0_AIPADDRATTR_AIPADDRDTR_Msk      (0x1u << XSPIMC0_AIPADDRATTR_AIPADDRDTR_Pos)
#define XSPIMC0_AIPADDRATTR_AIPADDRMODE_Pos     8u
#define XSPIMC0_AIPADDRATTR_AIPADDRMODE_Msk     (0xFu << XSPIMC0_AIPADDRATTR_AIPADDRMODE_Pos)

// AIPDIEPAGESTATE (0xAC) - derived from field widths (no explicit bracket comments)
#define XSPIMC0_AIPDIEPAGESTATE_AIPDIESELECTED_Pos 0u
#define XSPIMC0_AIPDIEPAGESTATE_AIPDIESELECTED_Msk (0x1u << XSPIMC0_AIPDIEPAGESTATE_AIPDIESELECTED_Pos)
#define XSPIMC0_AIPDIEPAGESTATE_AIPSELECTEDDIE_Pos 1u
#define XSPIMC0_AIPDIEPAGESTATE_AIPSELECTEDDIE_Msk (0x7Fu << XSPIMC0_AIPDIEPAGESTATE_AIPSELECTEDDIE_Pos)
#define XSPIMC0_AIPDIEPAGESTATE_AIPPAGESELECTED_Pos 8u
#define XSPIMC0_AIPDIEPAGESTATE_AIPPAGESELECTED_Msk (0x1u << XSPIMC0_AIPDIEPAGESTATE_AIPPAGESELECTED_Pos)
#define XSPIMC0_AIPDIEPAGESTATE_AIPSELECTEDPAGE_Pos 9u
#define XSPIMC0_AIPDIEPAGESTATE_AIPSELECTEDPAGE_Msk (0x7FFFFFu << XSPIMC0_AIPDIEPAGESTATE_AIPSELECTEDPAGE_Pos)

// AIPDUMMYATTR (0xB0)
#define XSPIMC0_AIPDUMMYATTR_AIPWLATENCYDYN_Pos 25u
#define XSPIMC0_AIPDUMMYATTR_AIPWLATENCYDYN_Msk (0x1u << XSPIMC0_AIPDUMMYATTR_AIPWLATENCYDYN_Pos)
#define XSPIMC0_AIPDUMMYATTR_AIPWLATENCYSKIP_Pos 24u
#define XSPIMC0_AIPDUMMYATTR_AIPWLATENCYSKIP_Msk (0x1u << XSPIMC0_AIPDUMMYATTR_AIPWLATENCYSKIP_Pos)
#define XSPIMC0_AIPDUMMYATTR_AIPWLATENCYLEN_Pos 16u
#define XSPIMC0_AIPDUMMYATTR_AIPWLATENCYLEN_Msk (0x1Fu << XSPIMC0_AIPDUMMYATTR_AIPWLATENCYLEN_Pos)
#define XSPIMC0_AIPDUMMYATTR_AIPRDUMMYSKIP_Pos  8u
#define XSPIMC0_AIPDUMMYATTR_AIPRDUMMYSKIP_Msk  (0x1u << XSPIMC0_AIPDUMMYATTR_AIPRDUMMYSKIP_Pos)
#define XSPIMC0_AIPDUMMYATTR_AIPRDUMMYLEN_Pos   0u
#define XSPIMC0_AIPDUMMYATTR_AIPRDUMMYLEN_Msk   (0x1Fu << XSPIMC_AIPDUMMYATTR_AIPRDUMMYLEN_Pos)

// AIPSUSRESCMD (0xB4)
#define XSPIMC0_AIPSUSRESCMD_AIPRESCMD_Pos      16u
#define XSPIMC0_AIPSUSRESCMD_AIPRESCMD_Msk      (0xFFFFu << XSPIMC0_AIPSUSRESCMD_AIPRESCMD_Pos)
#define XSPIMC0_AIPSUSRESCMD_AIPSUSCMD_Pos      0u
#define XSPIMC0_AIPSUSRESCMD_AIPSUSCMD_Msk      (0xFFFFu << XSPIMC0_AIPSUSRESCMD_AIPSUSCMD_Pos)

// AIPSUSRESATTR (0xB8)
#define XSPIMC0_AIPSUSRESATTR_SRCMDSD_Pos       13u
#define XSPIMC0_AIPSUSRESATTR_SRCMDSD_Msk       (0x1u << XSPIMC0_AIPSUSRESATTR_SRCMDSD_Pos)
#define XSPIMC0_AIPSUSRESATTR_SRCMDDDR_Pos      12u
#define XSPIMC0_AIPSUSRESATTR_SRCMDDDR_Msk      (0x1u << XSPIMC0_AIPSUSRESATTR_SRCMDDDR_Pos)
#define XSPIMC0_AIPSUSRESATTR_SRCMDMODE_Pos     8u
#define XSPIMC0_AIPSUSRESATTR_SRCMDMODE_Msk     (0xFu << XSPIMC0_AIPSUSRESATTR_SRCMDMODE_Pos)
#define XSPIMC0_AIPSUSRESATTR_SRCMDLEN_Pos      0u
#define XSPIMC0_AIPSUSRESATTR_SRCMDLEN_Msk      (0xFu << XSPIMC0_AIPSUSRESATTR_SRCMDLEN_Pos)

// AIPDATATTR (0xC0)
#define XSPIMC0_AIPDATATTR_AIPDATAKCH_Pos       16u
#define XSPIMC0_AIPDATATTR_AIPDATAKCH_Msk       (0x1u << XSPIMC0_AIPDATATTR_AIPDATAKCH_Pos)
#define XSPIMC0_AIPDATATTR_AIPDATAFB_Pos        7u
#define XSPIMC0_AIPDATATTR_AIPDATAFB_Msk        (0x1u << XSPIMC_AIPDATATTR_AIPDATAFB_Pos)
#define XSPIMC0_AIPDATATTR_AIPDATADQS_Pos       6u
#define XSPIMC0_AIPDATATTR_AIPDATADQS_Msk       (0x1u << XSPIMC0_AIPDATATTR_AIPDATADQS_Pos)
#define XSPIMC0_AIPDATATTR_AIPDATASD_Pos        5u
#define XSPIMC0_AIPDATATTR_AIPDATASD_Msk        (0x1u << XSPIMC0_AIPDATATTR_AIPDATASD_Pos)
#define XSPIMC0_AIPDATATTR_AIPDATADDR_Pos       4u
#define XSPIMC0_AIPDATATTR_AIPDATADDR_Msk       (0x1u << XSPIMC0_AIPDATATTR_AIPDATADDR_Pos)
#define XSPIMC0_AIPDATATTR_AIPDATAMODE_Pos      0u
#define XSPIMC0_AIPDATATTR_AIPDATAMODE_Msk      (0xFu << XSPIMC0_AIPDATATTR_AIPDATAMODE_Pos)

// AIPMODEBITSATTR (0xD4)
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSSKIP_Pos 16u
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSSKIP_Msk (0x1u << XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSSKIP_Pos)
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSSD_Pos  13u
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSSD_Msk  (0x1u << XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSSD_Pos)
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSDDR_Pos 12u
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSDDR_Msk (0x1u << XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSDDR_Pos)
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSMODE_Pos 8u
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSMODE_Msk (0xFu << XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSMODE_Pos)
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSLEN_Pos  0u
#define XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSLEN_Msk  (0x1Fu << XSPIMC0_AIPMODEBITSATTR_AIPMODEBITSLEN_Pos)

// AIPPRCMDATTR (0xD8)
#define XSPIMC0_AIPPRCMDATTR_ADDRESSWIDTH_Pos   17u
#define XSPIMC0_AIPPRCMDATTR_ADDRESSWIDTH_Msk   (0x3u << XSPIMC0_AIPPRCMDATTR_ADDRESSWIDTH_Pos)
#define XSPIMC0_AIPPRCMDATTR_PAGEENABLE_Pos     14u
#define XSPIMC0_AIPPRCMDATTR_PAGEENABLE_Msk     (0x1u << XSPIMC0_AIPPRCMDATTR_PAGEENABLE_Pos)
#define XSPIMC0_AIPPRCMDATTR_PRCMDSD_Pos        13u
#define XSPIMC0_AIPPRCMDATTR_PRCMDSD_Msk        (0x1u << XSPIMC0_AIPPRCMDATTR_PRCMDSD_Pos)
#define XSPIMC0_AIPPRCMDATTR_PRCMDDDR_Pos       12u
#define XSPIMC0_AIPPRCMDATTR_PRCMDDDR_Msk       (0x1u << XSPIMC0_AIPPRCMDATTR_PRCMDDDR_Pos)
#define XSPIMC0_AIPPRCMDATTR_PRCMDMODE_Pos      8u
#define XSPIMC0_AIPPRCMDATTR_PRCMDMODE_Msk      (0xFu << XSPIMC0_AIPPRCMDATTR_PRCMDMODE_Pos)
#define XSPIMC0_AIPPRCMDATTR_PRCMDLEN_Pos       0u
#define XSPIMC0_AIPPRCMDATTR_PRCMDLEN_Msk       (0xFu << XSPIMC0_AIPPRCMDATTR_PRCMDLEN_Pos)

// DELAY1 (CCDELAY1/AIPIDELAY1x)
#define XSPIMC0_DELAY1_DQS2DELAY_Pos            8u
#define XSPIMC0_DELAY1_DQS2DELAY_Msk            (0xFFu << XSPIMC0_DELAY1_DQS2DELAY_Pos)
#define XSPIMC0_DELAY1_DQS1DELAY_Pos            0u
#define XSPIMC0_DELAY1_DQS1DELAY_Msk            (0xFFu << XSPIMC0_DELAY1_DQS1DELAY_Pos)

// DELAY2 (CCDELAY2/AIPDELAY2x)
#define XSPIMC0_DELAY2_DQ3DELAY_Pos             24u
#define XSPIMC0_DELAY2_DQ3DELAY_Msk             (0xFFu << XSPIMC0_DELAY2_DQ3DELAY_Pos)
#define XSPIMC0_DELAY2_DQ2DELAY_Pos             16u
#define XSPIMC0_DELAY2_DQ2DELAY_Msk             (0xFFu << XSPIMC0_DELAY2_DQ2DELAY_Pos)
#define XSPIMC0_DELAY2_DQ1DELAY_Pos             8u
#define XSPIMC0_DELAY2_DQ1DELAY_Msk             (0xFFu << XSPIMC0_DELAY2_DQ1DELAY_Pos)
#define XSPIMC0_DELAY2_DQ0DELAY_Pos             0u
#define XSPIMC0_DELAY2_DQ0DELAY_Msk             (0xFFu << XSPIMC0_DELAY2_DQ0DELAY_Pos)

// DELAY3 (CCDELAY3/AIPDELAY3x)
#define XSPIMC0_DELAY3_DQ7DELAY_Pos             24u
#define XSPIMC0_DELAY3_DQ7DELAY_Msk             (0xFFu << XSPIMC0_DELAY3_DQ7DELAY_Pos)
#define XSPIMC0_DELAY3_DQ6DELAY_Pos             16u
#define XSPIMC0_DELAY3_DQ6DELAY_Msk             (0xFFu << XSPIMC0_DELAY3_DQ6DELAY_Pos)
#define XSPIMC0_DELAY3_DQ5DELAY_Pos             8u
#define XSPIMC0_DELAY3_DQ5DELAY_Msk             (0xFFu << XSPIMC0_DELAY3_DQ5DELAY_Pos)
#define XSPIMC0_DELAY3_DQ4DELAY_Pos             0u
#define XSPIMC0_DELAY3_DQ4DELAY_Msk             (0xFFu << XSPIMC0_DELAY3_DQ4DELAY_Pos)

// DELAY4 (CCDELAY4/AIPDELAY4x)
#define XSPIMC0_DELAY4_DQ11DELAY_Pos            24u
#define XSPIMC0_DELAY4_DQ11DELAY_Msk            (0xFFu << XSPIMC0_DELAY4_DQ11DELAY_Pos)
#define XSPIMC0_DELAY4_DQ10DELAY_Pos            16u
#define XSPIMC0_DELAY4_DQ10DELAY_Msk            (0xFFu << XSPIMC0_DELAY4_DQ10DELAY_Pos)
#define XSPIMC0_DELAY4_DQ9DELAY_Pos             8u
#define XSPIMC0_DELAY4_DQ9DELAY_Msk             (0xFFu << XSPIMC0_DELAY4_DQ9DELAY_Pos)
#define XSPIMC0_DELAY4_DQ8DELAY_Pos             0u
#define XSPIMC0_DELAY4_DQ8DELAY_Msk             (0xFFu << XSPIMC0_DELAY4_DQ8DELAY_Pos)

// DELAY5 (CCDELAY5/AIPDELAY5x)
#define XSPIMC0_DELAY5_DQ15DELAY_Pos            24u
#define XSPIMC0_DELAY5_DQ15DELAY_Msk            (0xFFu << XSPIMC0_DELAY5_DQ15DELAY_Pos)
#define XSPIMC0_DELAY5_DQ14DELAY_Pos            16u
#define XSPIMC0_DELAY5_DQ14DELAY_Msk            (0xFFu << XSPIMC0_DELAY5_DQ14DELAY_Pos)
#define XSPIMC0_DELAY5_DQ13DELAY_Pos            8u
#define XSPIMC0_DELAY5_DQ13DELAY_Msk            (0xFFu << XSPIMC0_DELAY5_DQ13DELAY_Pos)
#define XSPIMC0_DELAY5_DQ12DELAY_Pos            0u
#define XSPIMC0_DELAY5_DQ12DELAY_Msk            (0xFFu << XSPIMC0_DELAY5_DQ12DELAY_Pos)

// CSLPCLK (CSLPCLK0/CSLPCLK1)
#define XSPIMC0_CSLPCLK_CLOCKPOLARITY_Pos       17u
#define XSPIMC0_CSLPCLK_CLOCKPOLARITY_Msk       (0x1u << XSPIMC0_CSLPCLK_CLOCKPOLARITY_Pos)
#define XSPIMC0_CSLPCLK_TRANSMITDATAON_Pos      16u
#define XSPIMC0_CSLPCLK_TRANSMITDATAON_Msk      (0x1u << XSPIMC0_CSLPCLK_TRANSMITDATAON_Pos)
#define XSPIMC0_CSLPCLK_MINCSLOW_Pos            8u
#define XSPIMC0_CSLPCLK_MINCSLOW_Msk            (0xFFu << XSPIMC0_CSLPCLK_MINCSLOW_Pos)
#define XSPIMC0_CSLPCLK_CLOCKPERIOD_Pos         0u
#define XSPIMC0_CSLPCLK_CLOCKPERIOD_Msk         (0xFFu << XSPIMC0_CSLPCLK_CLOCKPERIOD_Pos)

// CSLPCS (CSLPCS0/CSLPCS1)
#define XSPIMC0_CSLPCS_LPEXITMINCSH_Pos         24u
#define XSPIMC0_CSLPCS_LPEXITMINCSH_Msk         (0xFFu << XSPIMC0_CSLPCS_LPEXITMINCSH_Pos)
#define XSPIMC0_CSLPCS_LPENTRYMINCSH_Pos        0u
#define XSPIMC0_CSLPCS_LPENTRYMINCSH_Msk        (0xFFu << XSPIMC0_CSLPCS_LPENTRYMINCSH_Pos)

// CSLPENTRYCMD1/CSLPENTRYCMD2
#define XSPIMC0_CSLPENTRYCMD1_LPENTRYCMD1_Pos   0u
#define XSPIMC0_CSLPENTRYCMD1_LPENTRYCMD1_Msk   (0xFFFFFFFFu << \
                                                 XSPIMC0_CSLPENTRYCMD1_LPENTRYCMD1_Pos)
#define XSPIMC0_CSLPENTRYCMD2_LPENTRYCMD2_Pos   0u
#define XSPIMC0_CSLPENTRYCMD2_LPENTRYCMD2_Msk   (0xFFFFFFFFu << \
                                                 XSPIMC0_CSLPENTRYCMD2_LPENTRYCMD2_Pos)

// CSLPEXITCMD1/CSLPEXITCMD2
#define XSPIMC0_CSLPEXITCMD1_LPEXITCMD1_Pos     0u
#define XSPIMC0_CSLPEXITCMD1_LPEXITCMD1_Msk     (0xFFFFFFFFu << \
                                                 XSPIMC0_CSLPEXITCMD1_LPEXITCMD1_Pos)
#define XSPIMC0_CSLPEXITCMD2_LPEXITCMD2_Pos     0u
#define XSPIMC0_CSLPEXITCMD2_LPEXITCMD2_Msk     (0xFFFFFFFFu << \
                                                 XSPIMC0_CSLPEXITCMD2_LPEXITCMD2_Pos)

// CSLPATTR (CSLPATTR0/CSLPATTR1)
#define XSPIMC0_CSLPATTR_USELPEXIT_Pos          17u
#define XSPIMC0_CSLPATTR_USELPEXIT_Msk          (0x1u << XSPIMC0_CSLPATTR_USELPEXIT_Pos)
#define XSPIMC0_CSLPATTR_USELPENTRY_Pos         16u
#define XSPIMC0_CSLPATTR_USELPENTRY_Msk         (0x1u << XSPIMC0_CSLPATTR_USELPENTRY_Pos)
#define XSPIMC0_CSLPATTR_STACKEDDIE_Pos         13u
#define XSPIMC0_CSLPATTR_STACKEDDIE_Msk         (0x1u << XSPIMC0_CSLPATTR_STACKEDDIE_Pos)
#define XSPIMC0_CSLPATTR_DDRMODE_Pos            12u
#define XSPIMC0_CSLPATTR_DDRMODE_Msk            (0x1u << XSPIMC0_CSLPATTR_DDRMODE_Pos)
#define XSPIMC0_CSLPATTR_MODE_Pos               8u
#define XSPIMC0_CSLPATTR_MODE_Msk               (0xFu << XSPIMC0_CSLPATTR_MODE_Pos)
#define XSPIMC0_CSLPATTR_DURATION_Pos           0u
#define XSPIMC0_CSLPATTR_DURATION_Msk           (0xFu << XSPIMC0_CSLPATTR_DURATION_Pos)

//! @} END_GROUP - xSPI Register Bitfield Definitions (CMSIS-style)

//*****************************************************************************
//
//! @name xSPI Interrupts
//! @brief Macro definitions for xSPI interrupt status bits.
//!
//! These macros correspond to the bits in the xSPI interrupt status register.
//! @{
//
//*****************************************************************************
// NOTE: AtomIQ110 exposes IRQ enables in `IRQCTRL` and latched status across
// `DMASTS` and `SPISTS`. We use the `IRQCTRL` bit positions as the public
// interrupt mask encoding in the HAL.
#define AM_HAL_XSPIMC_INT_DMAERR                 (1u << 18)
#define AM_HAL_XSPIMC_INT_DMA                    (1u << 17)
#define AM_HAL_XSPIMC_INT_DMA_IDLE               (1u << 16)

// FIFO level IRQ enables (threshold controlled by `IRQCTRL.FIFOBOUNDARY`)
#define AM_HAL_XSPIMC_INT_TX_FIFO_LVL            (1u << 2)
#define AM_HAL_XSPIMC_INT_RX_FIFO_LVL            (1u << 1)

// FIFO error IRQ enables
#define AM_HAL_XSPIMC_INT_TX_FIFO_ERR            (1u << 4)
#define AM_HAL_XSPIMC_INT_RX_FIFO_ERR            (1u << 3)

// Transaction state IRQ enable
#define AM_HAL_XSPIMC_INT_NOT_BUSY               (1u << 0)

// Backward-compatible aliases (no distinct 1/8-full encoding on AtomIQ110)
#define AM_HAL_XSPIMC_INT_RX_FIFO_1_8_FULL       AM_HAL_XSPIMC_INT_RX_FIFO_LVL
#define AM_HAL_XSPIMC_INT_RX_FIFO_FULL           AM_HAL_XSPIMC_INT_RX_FIFO_LVL
#define AM_HAL_XSPIMC_INT_TX_FIFO_1_8_FULL       AM_HAL_XSPIMC_INT_TX_FIFO_LVL
#define AM_HAL_XSPIMC_INT_TX_FIFO_FULL           AM_HAL_XSPIMC_INT_TX_FIFO_LVL
//! @} END_GROUP - xSPI Interrupts

//*****************************************************************************
//
//! @name Internal macros
//! @brief Internal macros to support CMSIS-Style macros
//! @{
//
//*****************************************************************************

/**
 * @brief Check whether a bit is set in a value.
 *
 * @param value Value that contains the bit.
 * @param bit Bit number (0 = LSB).
 *
 * @return true if the bit is set; false otherwise.
 */
#define IS_BIT_SET(value, bit) ((((value) >> (bit)) & (0x1)) != 0)

/** @brief Extract the Least Significant Bit from @p value. */
#define LSB_GET(value) ((value) & -(value))

/**
 * @brief Extract a bitfield element from @p value corresponding to
 *      the field mask @p mask.
 *
 * @param mask Bitmask for the field.
 * @param value Value containing the field.
 *
 * @return The field value, right-aligned.
 */
#define FIELD_GET(mask, value)  (((value) & (mask)) / LSB_GET(mask))

/**
 * @brief Prepare a bitfield element using @p value with @p mask representing
 *      its field position and width. The result should be combined
 *      with other fields using a logical OR.
 *
 * @param mask Bitmask for the field.
 * @param value Field value, right-aligned.
 *
 * @return The field value positioned and masked for insertion.
 */
#define FIELD_PREP(mask, value) (((value) * LSB_GET(mask)) & (mask))


// Helpers for safe macro argument expansion and token pasting
#define _EXPAND(x) x
#define CAT(a, b) _CAT2(a, b)

#define _CAT2(a, b) a ## b
#define _CAT3(a, b, c) a ## b ## c
#define _CAT4(a, b, c, d) a ## b ## c ## d

#define EXPAND2(a, b) _CAT2(a, b)
#define EXPAND3(a, b, c) _CAT3(a, b, c)
#define EXPAND4(a, b, c, d) _CAT4(a, b, c, d)

// Generic register/bitfield access helpers for XSPIMC_Type *
#define REG_FIELD_ID(moduleName, regName, bitName)    \
        EXPAND4(moduleName, regName, _, bitName)
#define REG_FIELD_POS(moduleName, regName, bitName)   \
        EXPAND2(REG_FIELD_ID(moduleName, regName, bitName), _Pos)
#define REG_FIELD_MSK(moduleName, regName, bitName)   \
        EXPAND2(REG_FIELD_ID(moduleName, regName, bitName), _Msk)

#define REG_BIT_WR(module, moduleName, regName, bitName, value)                                   \
        {                                                                                         \
            uint32_t _old  = REG_RD((module), regName);                                           \
            uint32_t _mask = REG_FIELD_MSK(moduleName, regName, bitName);                         \
            REG_WR((module), regName, (_old & ~_mask) | FIELD_PREP(_mask, (uint32_t)(value)));    \
        }

#define REG_WR(module, regName, value) \
        { _EXPAND(module)->_EXPAND(regName) = (value); }

#define REG_BIT_RD(module, moduleName, regName, bitName) \
        FIELD_GET(REG_FIELD_MSK(moduleName, regName, bitName), REG_RD((module), regName))

#define REG_RD(module, regName) \
        ( _EXPAND(module)->_EXPAND(regName))

#define XSPIMCn(n)                       ((XSPIMC_Type*)(XSPIMC ## n ## _BASE))

#define XSPIMC_IRQn(n)                         \
        ((n) == 0 ? XSPIMC ## 0 ## _IRQn :     \
         (n) == 1 ? XSPIMC ## 1 ## _IRQn :     \
         (n) == 2 ? XSPIMC ## 2 ## _IRQn :     \
         0u)

#define XSPIMCn_APERTURE_START_ADDR(n)                        \
        ((n) == 0 ? XSPIMC ## 0 ## _APERTURE_START_ADDR :     \
         (n) == 1 ? XSPIMC ## 1 ## _APERTURE_START_ADDR :     \
         (n) == 2 ? XSPIMC ## 2 ## _APERTURE_START_ADDR :     \
         0u)

#define XSPIMCn_APERTURE_END_ADDR(n)                          \
        ((n) == 0 ? XSPIMC ## 0 ## _APERTURE_END_ADDR :       \
         (n) == 1 ? XSPIMC ## 1 ## _APERTURE_END_ADDR :       \
         (n) == 2 ? XSPIMC ## 2 ## _APERTURE_END_ADDR :       \
         0u)

#define XSPIMC_REG_BIT_MASK(regName, bitName)      \
        REG_FIELD_MSK(XSPIMC0_, regName, bitName)

#define XSPIMC_REG_BIT_POS(regName, bitName)       \
        REG_FIELD_POS(XSPIMC0_, regName, bitName)

#define XSPIMCn_LABEL(n)                           \
        ((XSPIMC_Type*)(                           \
             (n) == 0 ? XSPIMC ## 0 ## _BASE :     \
             (n) == 1 ? XSPIMC ## 1 ## _BASE :     \
             (n) == 2 ? XSPIMC ## 2 ## _BASE :     \
             0u ))

#define XSPIMC_REG_BIT_WR(module, regName, bitName, value) \
        REG_BIT_WR(module, XSPIMC0_, regName, bitName, value)

 #define XSPIMC_REG_WR(module, regName, value)       \
         REG_WR(module, regName, value)

 #define XSPIMC_REG_BIT_RD(module, regName, bitName) \
         REG_BIT_RD(module, XSPIMC0_, regName, bitName)

 #define XSPIMC_REG_RD(module, regName)              \
         REG_RD(module, regName)

//! @} END_GROUP - Internal macros


//! @} END_GROUP - ll_xspi_macro_at110
typedef enum
{
    AM_HAL_XSPIMC_LL_IO_MODE_SINGLE = 1,
    AM_HAL_XSPIMC_LL_IO_MODE_DUAL = 2,
    AM_HAL_XSPIMC_LL_IO_MODE_QUAD = 4,
    AM_HAL_XSPIMC_LL_IO_MODE_OCTA = 8,
    AM_HAL_XSPIMC_LL_IO_MODE_HEX = 9,
} am_hal_xspimc_ll_io_mode_e;

typedef enum
{
    AM_HAL_XSPIMC_LL_DATA_WRITE = 0,
    AM_HAL_XSPIMC_LL_DATA_READ,
} am_hal_xspimc_ll_data_dir_e;

typedef enum
{
    AM_HAL_XSPIMC_LL_DMA_CMDLIST = 0,
    AM_HAL_XSPIMC_LL_DMA_LINEAR,
} am_hal_xspimc_ll_dma_mode_e;

typedef enum
{
    AM_HAL_XSPIMC_LL_DMA_CMD_LINK = 1,
    AM_HAL_XSPIMC_LL_DMA_CMD_DATA = 2,
} am_hal_xspimc_ll_dma_cmd_type_e;

typedef union
{
    volatile uint32_t xSPILPCmdAttr;
    struct
    {
        volatile uint32_t xSPILPCmdLen    : 4;
        uint32_t                          : 4;
        volatile uint32_t xSPILPCmdMode   : 4;
        volatile uint32_t xSPILPCmdDTR    : 1;
        volatile uint32_t xSPILPCmdSD     : 1;
        uint32_t                          : 2;
        volatile uint32_t xSPIUseLPEntry  : 1;
        volatile uint32_t xSPIUseLPExit   : 1;
        uint32_t                          : 14;
    } xSPILPCmdAttr_b;
} am_hal_xspimc_ll_lp_cmd_attr_t;

typedef union
{
    volatile uint32_t xSPICmdAttr;
    struct
    {
        volatile uint32_t xSPICmdLen        : 4;
        uint32_t                            : 4;
        volatile uint32_t xSPICmdMode       : 4;
        volatile uint32_t xSPICmdDTR        : 1;
        volatile uint32_t xSPICmdSD         : 1;
        uint32_t                            : 1;
        volatile uint32_t xSPICmdHRC        : 1;
        volatile uint32_t xSPICmdEN         : 1;
        uint32_t                            : 14;
        volatile uint32_t xSPICmdWriteErase : 1;
    } xSPICmdAttr_b;
} am_hal_xspimc_ll_cmd_attr_t;

typedef union
{
    volatile uint32_t xSPIAddrAttr;
    struct
    {
        volatile uint32_t xSPIAddrLen     : 5;
        uint32_t                          : 3;
        volatile uint32_t xSPIAddrMode    : 4;
        volatile uint32_t xSPIAddrDTR     : 1;
        volatile uint32_t xSPIAddrSD      : 1;
        uint32_t                          : 2;
        volatile uint32_t xSPIAddrEN      : 1;
        volatile uint32_t xSPIAddrMask    : 3;
        uint32_t                          : 4;
        volatile uint32_t xSPIAddrShift   : 2;
        uint32_t                          : 2;
        volatile uint32_t xSPIAddrTrans   : 3;
        uint32_t                          : 1;
    } xSPIAddrAttr_b;
} am_hal_xspimc_ll_addr_attr_t;

typedef union
{
    volatile uint32_t xSPIModeBitAttr;
    struct
    {
        volatile uint32_t xSPIModeBitLen  : 5;
        uint32_t                          : 3;
        volatile uint32_t xSPIModeBitMode : 4;
        volatile uint32_t xSPIModeBitDTR  : 1;
        volatile uint32_t xSPIModeBitSD   : 1;
        uint32_t                          : 2;
        volatile uint32_t xSPIModeBitEN   : 1;
        uint32_t                          : 15;
    } xSPIModeBitAttr_b;
} am_hal_xspimc_ll_modebit_attr_t;

typedef union
{
    volatile uint32_t xSPIDummyAttr;
    struct
    {
        volatile uint32_t xSPIRLC         : 5;
        uint32_t                          : 3;
        volatile uint32_t xSPIRLCEN       : 1;
        uint32_t                          : 7;
        volatile uint32_t xSPIWLC         : 5;
        uint32_t                          : 3;
        volatile uint32_t xSPIWLCEN       : 1;
        volatile uint32_t xSPIDYNWLCEN    : 1;
        volatile uint32_t xSPIDYNWLC      : 5;
        uint32_t                          : 1;
    } xSPIDummyAttr_b;
} am_hal_xspimc_ll_dummy_attr_t;

typedef union
{
    volatile uint32_t xSPIDataAttr;
    struct
    {
        volatile uint32_t xSPIDataMode    : 4;
        volatile uint32_t xSPIDataDTR     : 1;
        volatile uint32_t xSPIDataSD      : 1;
        volatile uint32_t xSPIDataDQS     : 1;
        volatile uint32_t xSPIDataFB      : 1;
        volatile uint32_t xSPIDataEN      : 1;
        uint32_t                          : 7;
        volatile uint32_t xSPIDataDir     : 1;
        volatile uint32_t xSPIDataKCH     : 1;
        uint32_t                          : 14;
    } xSPIDataAttr_b;
} am_hal_xspimc_ll_data_attr_t;

typedef union
{
    volatile uint32_t xAIPCMD;
    struct
    {
        volatile uint32_t xAIPRCMD        : 16;
        volatile uint32_t xAIPWCMD        : 16;
    } xAIPCMD_b;
} am_hal_xspimc_ll_aip_cmd_t;

typedef union
{
    volatile uint32_t xAIPSUSRESCMD;
    struct
    {
        volatile uint32_t xAIPSUSCMD      : 16;
        volatile uint32_t xAIPRESCMD      : 16;
    } xAIPSUSRESCMD_b;
} am_hal_xspimc_ll_aip_sus_res_cmd_t;

typedef union
{
    volatile uint32_t xAIPSUSRESATTR;
    struct
    {
        volatile uint32_t xAIPSRCMDLEN    : 4;
        uint32_t                          : 4;
        volatile uint32_t xAIPSRCmdMode   : 4;
        volatile uint32_t xAIPSRCmdDTR    : 1;
        volatile uint32_t xAIPSRCmdSD     : 1;
        uint32_t                          : 18;
    } xAIPSUSRESATTR_b;
} am_hal_xspimc_ll_aip_sus_res_attr_t;

typedef union
{
    volatile uint32_t xAIPADDRATTR;
    struct
    {
        uint32_t                          : 8;
        volatile uint32_t xAIPADDRMODE    : 4;          // [11:8]
        volatile uint32_t xAIPADDRDTR     : 1;          // [12]
        volatile uint32_t xAIPADDRSD      : 1;          // [13]
        uint32_t                          : 2;
        volatile uint32_t xAIPADDRWIDTH   : 2;          // [17:16]
        volatile uint32_t xAIPADDRMASK    : 3;          // [20:18]
        uint32_t                          : 3;
        volatile uint32_t xAIPADDRSHIFT   : 2;          // [25:24]
        uint32_t                          : 2;
        volatile uint32_t xAIPADDRTRANS   : 3;          // [30:28]
        uint32_t                          : 1;
    } xAIPADDRATTR_b;
} am_hal_xspimc_ll_aip_addr_attr_t;

typedef union
{
    volatile uint32_t xAIPDATAATTR;
    struct
    {
        volatile uint32_t xAIPDATAMODE    : 4;
        volatile uint32_t xAIPDATADDR     : 1;
        volatile uint32_t xAIPDATASD      : 1;
        volatile uint32_t xAIPDATADQS     : 1;
        volatile uint32_t xAIPDATAFB      : 1;
        uint32_t                          : 8;
        volatile uint32_t xAIPDATAKCH     : 1;
        uint32_t                          : 15;
    } xAIPDATAATTR_b;
} am_hal_xspimc_ll_aip_data_attr_t;

typedef union
{
    volatile uint32_t xSPIIRQCtl;
    struct
    {
        volatile uint32_t NOT_BUSY_EN     : 1;          // [0]: Not Busy IRQ Enable
        volatile uint32_t RFIFO_FILL_EN   : 1;          // [1]: Read FIFO Fill IRQ Enable
        volatile uint32_t WFIFO_FILL_EN   : 1;          // [2]: Write FIFO Fill IRQ Enable
        volatile uint32_t RFIFO_ERR_EN    : 1;          // [3]: Read FIFO Error IRQ Enable
        volatile uint32_t WFIFO_ERR_EN    : 1;          // [4]: Write FIFO Error IRQ Enable
        uint32_t                          : 11;
        volatile uint32_t DMA_IDLE_IRQ_EN : 1;          // [16]: DMA Idle IRQ Enable
        volatile uint32_t DMA_IRQ_EN      : 1;          // [17]: DMA IRQ enable
        volatile uint32_t DMA_ERR_EN      : 1;          // [18]: DMA Error IRQ enable
        uint32_t                          : 13;
    } xSPIIRQCtl_b;
} am_hal_xspimc_ll_irq_cfg_t;

typedef union
{
    volatile uint32_t xSPIDMACmdWord1;
    struct
    {
        volatile uint32_t LISTVALID       : 1;          // [0]: Must be 1
        volatile uint32_t LISTEND         : 1;          // [1]: The last item of a list
        volatile uint32_t IRQENABLE       : 1;          // [2]: IRQ enable
        uint32_t                          : 1;
        volatile uint32_t CMDTYPE         : 2;          // [5:4]: Type
        uint32_t                          : 10;
        volatile uint32_t DATABUFFSIZE    : 16;         // [31:16]: data buffer size in word
    } xSPIDMACmdWord1_b;
} am_hal_xspimc_ll_dma_cmd_word1_t;

typedef union
{
    volatile uint32_t xSPIDMACmdWord2;
    struct
    {
        uint32_t                          : 2;
        volatile uint32_t DATABUFFADDR    : 30;          // [31:2]: data buffer addr
    } xSPIDMACmdWord2_b;
} am_hal_xspimc_ll_dma_cmd_word2_t;

typedef struct
{
    uint32_t *pui32DataBuff;
    uint32_t ui32NumBytes;
    bool     bFinal;
    bool     bIRQ;
} am_hal_xspimc_ll_dma_data_des_t;

//*****************************************************************************
//
//! @brief xSPI Low level APIs
//! @defgroup ll_xspi_api_at110
//! @ingroup ll_xspi_at110
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Configure the xSPI clock phase, polarity, and divider.
//!
//! @param pReg            - xSPI register base pointer.
//! @param ui32CppMode     - clock phase/polarity mode selector (0..3).
//! @param ui32ClockPeriod - one-based clock divider.
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_clock_config(XSPIMC_Type *pReg,
                                          uint32_t ui32CppMode,
                                          uint32_t ui32ClockPeriod);

//*****************************************************************************
//
//! @brief Configure chip select timing and select active chip selects.
//!
//! @param pReg          - xSPI register base pointer.
//! @param ui32CE        - chip select mask.
//! @param ui32MinCELow  - minimum CS low time (hardware-encoded, N cycles).
//! @param ui32MinCEHigh - minimum CS high time (hardware-encoded, N cycles).
//
//*****************************************************************************
extern void am_hal_xspimc_ll_ce_config(XSPIMC_Type *pReg,
                                       uint32_t ui32CE,
                                       uint32_t ui32MinCELow,
                                       uint32_t ui32MinCEHigh);

//*****************************************************************************
//
//! @brief Configure interrupt enables (ORed into IRQCTRL).
//!
//! @param pReg   - xSPI register base pointer.
//! @param irqCfg - interrupt enable configuration bitfield.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_irq_config(XSPIMC_Type *pReg, am_hal_xspimc_ll_irq_cfg_t irqCfg);

//*****************************************************************************
//
//! @brief Configure FIFO boundary level used by FIFO level interrupts.
//!
//! @param pReg        - xSPI register base pointer.
//! @param ui32FifoLvl - FIFO boundary selector (hardware field encoding).
//
//*****************************************************************************
extern void am_hal_xspimc_ll_fifo_config(XSPIMC_Type *pReg, uint32_t ui32FifoLvl);

// IRQ status clear helpers (W1C on relevant status bits)
//*****************************************************************************
//
//! @brief Clear selected interrupt/status sources (W1C where applicable).
//!
//! @param pReg        - xSPI register base pointer.
//! @param ui32IntMask - interrupt mask (AM_HAL_XSPIMC_INT_* bits).
//
//*****************************************************************************
extern void am_hal_xspimc_ll_irq_status_clear(XSPIMC_Type *pReg, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Read the IRQCTRL register.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return Current IRQCTRL value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_irq_get(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Read the SPIDUMMYATTR register.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return SPIDUMMYATTR register value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_dummy_attr_get(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Read the SPIDATAATTR register.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return SPIDATAATTR register value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_data_attr_get(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Read the SPIMODEBITSATTR register.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return SPIMODEBITSATTR register value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_modebits_attr_get(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Read the SPICMDATTR register.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return SPICMDATTR register value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_cmd_attr_get(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Read the SPIADDRATTR register.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return SPIADDRATTR register value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_addr_attr_get(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Program SPI address register (SPIADDR).
//!
//! @param pReg     - xSPI register base pointer.
//! @param ui32Addr - address to program.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_spiaddr_set(XSPIMC_Type *pReg, uint32_t ui32Addr);

//*****************************************************************************
//
//! @brief Program SPIDATASIZE as a number of bits.
//!
//! @param pReg        - xSPI register base pointer.
//! @param ui32NumBits - number of data bits to transfer (0 disables).
//
//*****************************************************************************
extern void am_hal_xspimc_ll_datasize_set_bits(XSPIMC_Type *pReg, uint32_t ui32NumBits);

//*****************************************************************************
//
//! @brief Check whether the controller is busy.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return true if busy; false if idle.
//
//*****************************************************************************
extern bool am_hal_xspimc_ll_is_busy(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Check whether the controller FIFO is empty.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return true if empty; false if not empty.
//
//*****************************************************************************
extern bool am_hal_xspimc_ll_is_fifo_empty(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Configure sampling point selection.
//!
//! @param pReg            - xSPI register base pointer.
//! @param ui32SamplePoint - one-based sample count (1..256).
//!
//! The caller validates the count; the register stores count minus one.
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_sample_point_config(XSPIMC_Type *pReg,
                                                 uint32_t ui32SamplePoint);

//*****************************************************************************
//
//! @brief Get sampling point value.
//!
//! @param pReg            - xSPI register base pointer.
//!
//! @return Sample point value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_sample_point_get(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Configure SPI custom command interface delay settings.
//!
//! @param pDelay          - CCDDELAY register base pointer.
//! @param ui32DQSDelay    - DQS delay setting (hardware-encoded).
//! @param ui32DataDelay   - data delay setting (hardware-encoded).
//*****************************************************************************
extern void am_hal_xspimc_ll_delay_config(XSPIMC_DELAY_Type *pDelay,
                                          uint32_t ui32DQSDelay, uint32_t ui32DataDelay);

//*****************************************************************************
//
//! @brief Configure the custom SPI output hold time before the dummy phase.
//!
//! @param pReg         - xSPI register base pointer.
//! @param ui32HoldTime - hardware hold-time encoding (0..255).
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_hold_time_config(XSPIMC_Type *pReg, uint32_t ui32HoldTime);

//*****************************************************************************
//
//! @brief Configure both DQS input delays for custom SPI or AIP.
//!
//! @param pReg          - base pointer to the five-register delay block.
//! @param ui32DQS0Delay - input delay for DQS0 (0..2^IMPDELAY).
//! @param ui32DQS1Delay - input delay for DQS1 (0..2^IMPDELAY).
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_delay1_config(XSPIMC_DELAY_Type *pReg, uint32_t ui32DQS0Delay,
                                           uint32_t ui32DQS1Delay);

//*****************************************************************************
//
//! @brief Configure DQ0 through DQ3 input delays for custom SPI or AIP.
//!
//! @param pReg           - base pointer to the five-register delay block.
//! @param ui32Data0Delay - delay for DQ0 (0..2^IMPDELAY).
//! @param ui32Data1Delay - delay for DQ1 (0..2^IMPDELAY).
//! @param ui32Data2Delay - delay for DQ2 (0..2^IMPDELAY).
//! @param ui32Data3Delay - delay for DQ3 (0..2^IMPDELAY).
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_delay2_config(XSPIMC_DELAY_Type *pReg, uint32_t ui32Data0Delay,
                                           uint32_t ui32Data1Delay, uint32_t ui32Data2Delay,
                                           uint32_t ui32Data3Delay);

//*****************************************************************************
//
//! @brief Configure DQ4 through DQ7 input delays for custom SPI or AIP.
//!
//! @param pReg           - base pointer to the five-register delay block.
//! @param ui32Data4Delay - delay for DQ4 (0..2^IMPDELAY).
//! @param ui32Data5Delay - delay for DQ5 (0..2^IMPDELAY).
//! @param ui32Data6Delay - delay for DQ6 (0..2^IMPDELAY).
//! @param ui32Data7Delay - delay for DQ7 (0..2^IMPDELAY).
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_delay3_config(XSPIMC_DELAY_Type *pReg, uint32_t ui32Data4Delay,
                                           uint32_t ui32Data5Delay, uint32_t ui32Data6Delay,
                                           uint32_t ui32Data7Delay);

//*****************************************************************************
//
//! @brief Configure DQ8 through DQ11 input delays for custom SPI or AIP.
//!
//! @param pReg            - base pointer to the five-register delay block.
//! @param ui32Data8Delay  - delay for DQ8 (0..2^IMPDELAY).
//! @param ui32Data9Delay  - delay for DQ9 (0..2^IMPDELAY).
//! @param ui32Data10Delay - delay for DQ10 (0..2^IMPDELAY).
//! @param ui32Data11Delay - delay for DQ11 (0..2^IMPDELAY).
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_delay4_config(XSPIMC_DELAY_Type *pReg, uint32_t ui32Data8Delay,
                                           uint32_t ui32Data9Delay, uint32_t ui32Data10Delay,
                                           uint32_t ui32Data11Delay);

//*****************************************************************************
//
//! @brief Configure DQ12 through DQ15 input delays for custom SPI or AIP.
//!
//! @param pReg            - base pointer to the five-register delay block.
//! @param ui32Data12Delay - delay for DQ12 (0..2^IMPDELAY).
//! @param ui32Data13Delay - delay for DQ13 (0..2^IMPDELAY).
//! @param ui32Data14Delay - delay for DQ14 (0..2^IMPDELAY).
//! @param ui32Data15Delay - delay for DQ15 (0..2^IMPDELAY).
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_delay5_config(XSPIMC_DELAY_Type *pReg, uint32_t ui32Data12Delay,
                                           uint32_t ui32Data13Delay, uint32_t ui32Data14Delay,
                                           uint32_t ui32Data15Delay);

//*****************************************************************************
//
//! @brief Configure AIP sampling point.
//!
//! @param pAip            - AIP window register base pointer.
//! @param ui32SamplePoint - one-based sample count (1..256).
//!
//! The caller validates the count; the register stores count minus one.
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_sample_point_config(AIP_Type *pAip,
                                                     uint32_t ui32SamplePoint);

//*****************************************************************************
//
//! @brief Get AIP sampling point value.
//!
//! @param pAip            - AIP window register base pointer.
//!
//! @return Sample point value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_aip_sample_point_get(AIP_Type *pAip);

//*****************************************************************************
//
//! @brief Configure the AIP output hold time before the dummy phase.
//!
//! @param pAip            - AIP window register base pointer.
//! @param ui32HoldTime    - hardware hold-time encoding (0..255).
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_hold_time_config(AIP_Type *pAip, uint32_t ui32HoldTime);

//*****************************************************************************
//
//! @brief Program the SPI command phase attributes (SPICMDATTR).
//!
//! @param pReg    - xSPI register base pointer.
//! @param cmdAttr - command phase attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_cmd_phase_config(XSPIMC_Type *pReg,
                                              am_hal_xspimc_ll_cmd_attr_t cmdAttr);

//*****************************************************************************
//
//! @brief Program the SPI address phase attributes (SPIADDRATTR).
//!
//! @param pReg     - xSPI register base pointer.
//! @param addrAttr - address phase attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_address_phase_config(XSPIMC_Type *pReg,
                                                  am_hal_xspimc_ll_addr_attr_t addrAttr);

//*****************************************************************************
//
//! @brief Program the SPI mode bits phase attributes (SPIMODEBITSATTR).
//!
//! @param pReg     - xSPI register base pointer.
//! @param modeAttr - mode bits attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_mode_phase_config(XSPIMC_Type *pReg,
                                               am_hal_xspimc_ll_modebit_attr_t modeAttr);

//*****************************************************************************
//
//! @brief Program the SPI dummy phase attributes (SPIDUMMYATTR).
//!
//! @param pReg      - xSPI register base pointer.
//! @param dummyAttr - dummy phase attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_dummy_phase_config(XSPIMC_Type *pReg,
                                                am_hal_xspimc_ll_dummy_attr_t dummyAttr);

//*****************************************************************************
//
//! @brief Program the SPI data phase attributes (SPIDATAATTR).
//!
//! @param pReg     - xSPI register base pointer.
//! @param dataAttr - data phase attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_data_phase_config(XSPIMC_Type *pReg,
                                               am_hal_xspimc_ll_data_attr_t dataAttr);

//*****************************************************************************
//
//! @brief Program the mode bits value (SPIMODEBITS).
//!
//! @param pReg         - xSPI register base pointer.
//! @param ui32ModeBits - mode bits value.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_modebits_config(XSPIMC_Type *pReg, uint32_t ui32ModeBits);

//*****************************************************************************
//
//! @brief Start an SPI transaction by writing SPICMD.
//!
//! @param pReg    - xSPI register base pointer.
//! @param ui32Cmd - command value to start.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_cmd_start(XSPIMC_Type *pReg, uint32_t ui32Cmd);

//*****************************************************************************
//
//! @brief Perform a PIO transmit (write) transaction.
//!
//! @param pReg         - xSPI register base pointer.
//! @param ui32Cmd      - command value.
//! @param pui8Data     - pointer to TX data buffer.
//! @param ui32NumBytes - number of bytes to transmit.
//!
//! @return Number of bytes transmitted.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_pio_tx(XSPIMC_Type *pReg,
                                        uint32_t ui32Cmd,
                                        const uint8_t *pui8Data,
                                        uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Perform a PIO receive (read) transaction.
//!
//! @param pReg         - xSPI register base pointer.
//! @param ui32Cmd      - command value.
//! @param pui8Data     - pointer to RX buffer.
//! @param ui32NumBytes - number of bytes to receive.
//!
//! @return Number of bytes received.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_pio_rx(XSPIMC_Type *pReg,
                                        uint32_t ui32Cmd,
                                        uint8_t *pui8Data,
                                        uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Query hardware capability block and version.
//!
//! @param pReg         - xSPI register base pointer.
//! @param pui32Version - output: version register value.
//! @param ppCaps       - output: pointer to the capability structure.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_get_caps(XSPIMC_Type *pReg, uint32_t *pui32Version,
                                      CAPS_TYPE **ppCaps);

//*****************************************************************************
//
//! @brief Configure DMA mode and burst length.
//!
//! @param pReg         - xSPI register base pointer.
//! @param ui32BurstLen - burst length selector (hardware field encoding).
//
//*****************************************************************************
extern void am_hal_xspimc_ll_dma_burst_config(XSPIMC_Type *pReg,
                                              uint32_t ui32BurstLen);

//*****************************************************************************
//
//! @brief Configure a linear DMA transfer.
//!
//! @param pReg         - xSPI register base pointer.
//! @param pui32Data    - data buffer (word-aligned pointer).
//
//*****************************************************************************
extern void am_hal_xspimc_ll_dma_setup_linear(XSPIMC_Type *pReg,
                                              uint32_t *pui32Data);

//*****************************************************************************
//
//! @brief Configure a command-list DMA transfer.
//!
//! @param pReg         - xSPI register base pointer.
//! @param pui32CmdList - pointer to the command list.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_dma_setup_linked(XSPIMC_Type *pReg, uint32_t *pui32CmdList);

//*****************************************************************************
//
//! @brief Disable DMA
//!
//! @param pReg          - xSPI register base pointer.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_dma_disable(XSPIMC_Type *pReg);

//*****************************************************************************
//! @brief Configure the WP pin (output bit 2).
//!
//! @param pReg          - xSPI register base pointer.
//! @param bDrive        - true to drive WP; false to leave WP high impedance.
//! @param bState        - output level when bDrive is true.
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_wp_ctrl(XSPIMC_Type *pReg, bool bDrive, bool bState);

//*****************************************************************************
//! @brief Configure the Hold pin (output bit 3).
//!
//! @param pReg          - xSPI register base pointer.
//! @param bDrive        - true to drive HOLD; false to leave HOLD high impedance.
//! @param bState        - output level when bDrive is true.
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_hold_ctrl(XSPIMC_Type *pReg, bool bDrive, bool bState);

//*****************************************************************************
//
//! @brief Get composed interrupt/status bits.
//!
//! @param pReg        - xSPI register base pointer.
//! @param pui32Status - output: composed status mask (AM_HAL_XSPIMC_INT_*).
//! @param bEnableOnly - true to filter to enabled interrupts only.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_irq_status_get(XSPIMC_Type *pReg,
                                            uint32_t *pui32Status,
                                            bool bEnableOnly);

//*****************************************************************************
//
//! @brief Configure the low-power mode clock settings.
//!
//! @param pLp             - low-power mode register block pointer.
//! @param ui32CppMode     - clock phase/polarity mode selector (implementation-defined).
//! @param ui32ClockPeriod - clock period divider (hardware-encoded value).
//*****************************************************************************
extern void am_hal_xspimc_ll_lp_clock_config(CSLP_Type *pLp,
                                             uint32_t ui32CppMode,
                                             uint32_t ui32ClockPeriod);

//*****************************************************************************
//
//! @brief Configure the low-power mode timing settings.
//!
//! @param pLp                - low-power mode register block pointer.
//! @param ui32MinCELow       - minimum CS low time (hardware-encoded, N cycles).
//! @param ui32ExitMinCEHigh  - minimum CS high time for exit (hardware-encoded, N cycles).
//! @param ui32EnterMinCEHigh - minimum CS high time for entry (hardware-encoded, N cycles).
//*****************************************************************************
extern void am_hal_xspimc_ll_lp_timing_config(CSLP_Type *pLp,
                                              uint32_t ui32MinCELow,
                                              uint32_t ui32ExitMinCEHigh,
                                              uint32_t ui32EnterMinCEHigh);

//*****************************************************************************
//
//! @brief Configure the low-power mode command attributes.
//!
//! @param pLp       - low-power mode register block pointer.
//! @param lpAttr    - low-power mode command attribute union.
//*****************************************************************************
extern void am_hal_xspimc_ll_lp_cmd_attr_config(CSLP_Type *pLp,
                                                am_hal_xspimc_ll_lp_cmd_attr_t lpAttr);

//*****************************************************************************
//
//! @brief Configure the low-power mode entry commands.
//!
//! @param pLp       - low-power mode register block pointer.
//! @param ui32Cmd1  - first word of entry command
//! @param ui32Cmd2  - second word of entry command
//*****************************************************************************
extern void am_hal_xspimc_ll_lp_entry_cmd_config(CSLP_Type *pLp,
                                                 uint32_t ui32Cmd1, uint32_t ui32Cmd2);

//*****************************************************************************
//
//! @brief Configure the low-power mode exit commands.
//!
//! @param pLp       - low-power mode register block pointer.
//! @param ui32Cmd1  - first word of exit command
//! @param ui32Cmd2  - second word of exit command
//*****************************************************************************
extern void am_hal_xspimc_ll_lp_exit_cmd_config(CSLP_Type *pLp,
                                                uint32_t ui32Cmd1, uint32_t ui32Cmd2);

//*****************************************************************************
//
//! @brief Get the low-power mode register block pointer.
//!
//! @param pReg - xSPI register base pointer.
//! @param ui32CE - chip select index (0-based).
//!
//! @return Pointer to low-power mode register block, or NULL if invalid.
//*****************************************************************************
extern CSLP_Type *am_hal_xspimc_ll_lp_get(XSPIMC_Type *pReg, uint32_t ui32CE);

//*****************************************************************************
//
//! @brief Get the AIP window delay register block pointer.
//!
//! @param pReg          - xSPI register base pointer.
//! @param ui32AipWindow - AIP window index.
//!
//! @return Pointer to AIP window delay register block, or NULL if invalid.
//*****************************************************************************
extern XSPIMC_DELAY_Type *am_hal_xspimc_ll_aip_window_delay_get(XSPIMC_Type *pReg,
                                                                uint32_t ui32AipWindow);

//*****************************************************************************
//
//! @brief Get the AIP window register block pointer.
//!
//! @param pReg          - xSPI register base pointer.
//! @param ui32AipWindow - AIP window index.
//!
//! @return Pointer to AIP window register block, or NULL if invalid.
//
//*****************************************************************************
extern AIP_Type *am_hal_xspimc_ll_aip_window_get(XSPIMC_Type *pReg, uint32_t ui32AipWindow);

//*****************************************************************************
//
//! @brief Configure the AIP window clock phase, polarity, and divider.
//!
//! @param pAip            - AIP window register base pointer.
//! @param ui32CppMode     - clock phase/polarity mode selector (0..3).
//! @param ui32ClockPeriod - one-based clock divider.
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_clock_config(AIP_Type *pAip,
                                              uint32_t ui32CppMode,
                                              uint32_t ui32ClockPeriod);

//*****************************************************************************
//
//! @brief Configure AIP chip select timing and selection.
//!
//! @param pAip          - AIP window register base pointer.
//! @param ui32CE        - chip select mask.
//! @param ui32MinCELow  - minimum CS low time (hardware-encoded, N cycles).
//! @param ui32MinCEHigh - minimum CS high time (hardware-encoded, N cycles).
//! @param ui32MinCEHigh - maximum CS low time (hardware-encoded, N cycles).
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_ce_config(AIP_Type *pAip,
                                           uint32_t ui32CE,
                                           uint32_t ui32MinCELow,
                                           uint32_t ui32MinCEHigh,
                                           uint32_t ui32MaxCELow);

//*****************************************************************************
//
//! @brief Configure the AIP access boundary.
//!
//! @param pAip        - AIP window register base pointer.
//! @param ui32Boundary - boundary value
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_access_boundary_config(AIP_Type *pAip, uint32_t ui32Boundary);

//*****************************************************************************
//
//! @brief Program the AIP read/write command register.
//!
//! @param pAip - AIP window register base pointer.
//! @param cmd  - command union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_cmd_config(AIP_Type *pAip, am_hal_xspimc_ll_aip_cmd_t cmd);

//*****************************************************************************
//
//! @brief Program the AIP command attribute register.
//!
//! @param pAip       - AIP window register base pointer.
//! @param cmdAttrWord - command attribute union (same encoding as SPI command attributes).
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_cmd_attr_config(AIP_Type *pAip,
                                                 am_hal_xspimc_ll_cmd_attr_t cmdAttrWord);

//*****************************************************************************
//
//! @brief Program the AIP address attribute register.
//!
//! @param pAip         - AIP window register base pointer.
//! @param addrAttrWord - AIP address attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_addr_attr_config(AIP_Type *pAip,
                                                  am_hal_xspimc_ll_aip_addr_attr_t addrAttrWord);

//*****************************************************************************
//
//! @brief Program the AIP data attribute register.
//!
//! @param pAip         - AIP window register base pointer.
//! @param dataAttrWord - AIP data attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_data_attr_config(AIP_Type *pAip,
                                                  am_hal_xspimc_ll_aip_data_attr_t dataAttrWord);

//*****************************************************************************
//
//! @brief Program the AIP dummy attribute register.
//!
//! @param pAip      - AIP window register base pointer.
//! @param dummyAttr - dummy attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_dummy_attr_config(AIP_Type *pAip,
                                                   am_hal_xspimc_ll_dummy_attr_t dummyAttr);

//*****************************************************************************
//
//! @brief Program the AIP mode bits attribute register.
//!
//! @param pAip     - AIP window register base pointer.
//! @param modeAttr - mode bits attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_modebits_attr_config(AIP_Type *pAip,
                                                      am_hal_xspimc_ll_modebit_attr_t modeAttr);

//*****************************************************************************
//
//! @brief Program the AIP mode bits value register.
//!
//! @param pAip         - AIP window register base pointer.
//! @param ui32ModeBits - mode bits value.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_modebits_config(AIP_Type *pAip, uint32_t ui32ModeBits);

//*****************************************************************************
//
//! @brief Configure AIP read/write permissions.
//!
//! @param pAip - AIP window register base pointer.
//! @param bRW  - true to allow writes; false for read-only.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_rw_config(AIP_Type *pAip, bool bRW);

//*****************************************************************************
//
//! @brief Enable or disable AIP prefetch.
//!
//! @param pReg    - AIP window register base pointer.
//! @param bEnable - true to enable prefetch; false to disable.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_prefetch_config(AIP_Type *pReg, bool bEnable);

//*****************************************************************************
//
//! @brief Enable or disable AIP automatic suspend/resume support.
//!
//! @param pAip    - AIP window register base pointer.
//! @param bEnable - true to enable; false to disable.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_susres_config(AIP_Type *pAip, bool bEnable);

//*****************************************************************************
//
//! @brief Program the AIP suspend/resume command opcode register.
//!
//! @param pAip - AIP window register base pointer.
//! @param cmd  - suspend/resume command union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_sus_cmd_config(AIP_Type *pAip,
                                                am_hal_xspimc_ll_aip_sus_res_cmd_t cmd);

//*****************************************************************************
//
//! @brief Program the AIP suspend/resume command attribute register.
//!
//! @param pAip  - AIP window register base pointer.
//! @param attr  - suspend/resume attribute union.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_susres_attr_config(AIP_Type *pAip,
                                                    am_hal_xspimc_ll_aip_sus_res_attr_t attr);

//*****************************************************************************
//
//! @brief Program the AIP suspend time register.
//!
//! @param pAip      - AIP window register base pointer.
//! @param ui32Time  - time in bus clock cycles.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_suspend_time_config(AIP_Type *pAip, uint32_t ui32Time);

//*****************************************************************************
//
//! @brief Program the AIP resume time register.
//!
//! @param pAip      - AIP window register base pointer.
//! @param ui32Time  - time in bus clock cycles.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_resume_time_config(AIP_Type *pAip, uint32_t ui32Time);

//*****************************************************************************
//
//! @brief Program the AIP suspend-to-resume delay register.
//!
//! @param pAip      - AIP window register base pointer.
//! @param ui32Time  - time in bus clock cycles.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_sus2res_time_config(AIP_Type *pAip, uint32_t ui32Time);

//*****************************************************************************
//
//! @brief Program the AIP resume-to-suspend delay register.
//!
//! @param pAip      - AIP window register base pointer.
//! @param ui32Time  - time in bus clock cycles.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_res2sus_time_config(AIP_Type *pAip, uint32_t ui32Time);

//*****************************************************************************
//
//! @brief Latch AIP configuration changes.
//!
//! @param pAip - AIP window register base pointer.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_update(AIP_Type *pAip);

//*****************************************************************************
//
//! @brief Enable or disable AIP (XIP) mode.
//!
//! @param pAip    - AIP window register base pointer.
//! @param bEnable - true to enable; false to disable.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_mode_enable(AIP_Type *pAip, bool bEnable);

//*****************************************************************************
//
//! @brief Check if AIP (XIP) mode is enabled.
//!
//! @param pAip - AIP window register base pointer.
//!
//! @return true if enabled; false otherwise.
//
//*****************************************************************************
extern bool am_hal_xspimc_ll_aip_is_enabled(AIP_Type *pAip);

//*****************************************************************************
//
//! @brief Check whether an AIP window is busy.
//!
//! @param pAip - AIP window register base pointer.
//!
//! @return true if busy; false otherwise.
//
//*****************************************************************************
extern bool am_hal_xspimc_ll_aip_is_busy(AIP_Type *pAip);

//*****************************************************************************
//
//! @brief Configure AIP window base address and size.
//!
//! @param pAip      - AIP window register base pointer.
//! @param ui32Addr  - base address.
//! @param ui32Size  - size encoding.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_window_config(AIP_Type *pAip, uint32_t ui32WinAddr,
                                               uint32_t ui32Addr, uint32_t ui32Size);

//*****************************************************************************
//
//! @brief Configure AIP window delay settings.
//!
//! @param pDelay          - AIP window delay register base pointer.
//! @param ui32DQSDelay    - DQS delay setting (hardware-encoded).
//! @param ui32DataDelay   - data delay setting (hardware-encoded).
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_delay_config(XSPIMC_DELAY_Type *pDelay,
                                              uint32_t ui32DQSDelay, uint32_t ui32DataDelay);

//*****************************************************************************
//
//! @brief Configure the global AIP read-while-write holdoff timer.
//!
//! @param pReg        - xSPI register base pointer.
//! @param ui32Time    - holdoff time in controller clock cycles (hardware max
//!                      is the AIPHOLDOFFTIME field width).
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_holdoff_time_config(XSPIMC_Type *pReg, uint32_t ui32Time);

//*****************************************************************************
//
//! @brief Select which AIP windows participate in read-while-write holdoff.
//!
//! @param pReg        - xSPI register base pointer.
//! @param ui32WinMask - per-window enable mask, bit n selects AIP window n.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_holdoff_select_config(XSPIMC_Type *pReg, uint32_t ui32WinMask);

//*****************************************************************************
//
//! @brief Read the AIP holdoff select register.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return Raw AIPHOLDOFFSEL register value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_aip_holdoff_select_get(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Read the AIP holdoff control/status register.
//!
//! @param pReg - xSPI register base pointer.
//!
//! @return Raw AIPHOLDOFFCTRL register value.
//
//*****************************************************************************
extern uint32_t am_hal_xspimc_ll_aip_holdoff_ctrl_get(XSPIMC_Type *pReg);

//*****************************************************************************
//
//! @brief Write the AIP holdoff control register (start/cancel holdoff).
//!
//! @param pReg        - xSPI register base pointer.
//! @param ui32Ctrl    - control word to write to AIPHOLDOFFCTRL.
//
//*****************************************************************************
extern void am_hal_xspimc_ll_aip_holdoff_ctrl_set(XSPIMC_Type *pReg, uint32_t ui32Ctrl);

//! @} END_GROUP - xSPI Low level APIs.

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_XSPIMC_LL_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
