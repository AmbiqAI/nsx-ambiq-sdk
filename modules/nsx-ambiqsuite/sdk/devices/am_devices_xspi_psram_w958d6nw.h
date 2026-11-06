//*****************************************************************************
//
//! @file am_devices_xspi_psram_w958d6nw.h
//!
//! @brief Winbond W958D6NW xSPI PSRAM driver.
//!
//! @addtogroup devices_xspi_psram_w958d6nw W958D6NW xSPI PSRAM Driver
//! @ingroup devices
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

#ifndef AM_DEVICES_XSPI_PSRAM_W958D6NW_H
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_H

#include "am_devices_xspi.h"

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name Global definitions
//! @{
//
//*****************************************************************************
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_GLOBAL_RAM_SIZE  172

//! @}

//*****************************************************************************
//
//! @name Global definitions for psram commands
//! @{
//
//*****************************************************************************
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_DDR_READ             0x0005 //0xA000
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_DDR_WRITE            0x0001 //0x2000
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_DDR_READ_REGISTER    0x0007 //0xE000
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_DDR_WRITE_REGISTER   0x0003 //0x6000

#define AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ID0_ADDR         0x0000
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ID1_ADDR         0x0001
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_CR0_ADDR         0x0800
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_CR1_ADDR         0x0801

#define AM_DEVICES_XSPI_PSRAM_W958D6NW_REG_ADDR_LEN         4

//! @}

//*****************************************************************************
//
//! @name Device specific definitions
//! @{
//
//*****************************************************************************
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_PAGE_SIZE            1024

#define AM_DEVICES_XSPI_PSRAM_W958D6NW_TVCS_US              150
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_THS_US               3
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_TEXTHS_US            100

#define AM_DEVICES_XSPI_PSRAM_W958D6NW_VENDOR_ID            0x6
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_DEVICE_ID            0x9
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_ROW_BITS             0xE
#define AM_DEVICES_XSPI_PSRAM_W958D6NW_COL_BITS             0x7

//! @}

//*****************************************************************************
//
//! @name Type definitions
//! @{
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_5 = 0x0,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_6 = 0x1,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_7 = 0x2,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_3 = 0xE,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_ILC_4 = 0xF,
} am_devices_xspi_psram_w958d6nw_ilc_e;

typedef enum
{
    AM_DEVICES_XSPI_PSRAM_W958D6NW_DS_34_OHM_0,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_DS_115_OHM,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_DS_67_OHM,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_DS_46_OHM,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_DS_34_OHM_1,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_DS_27_OHM,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_DS_22_OHM,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_DS_19_OHM,
} am_devices_xspi_psram_w958d6nw_ds_e;

typedef enum
{
    AM_DEVICES_XSPI_PSRAM_W958D6NW_BL_128,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_BL_64,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_BL_16,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_BL_32,
} am_devices_xspi_psram_w958d6nw_bl_e;

typedef enum
{
    AM_DEVICES_XSPI_PSRAM_W958D6NW_PAR_FULL,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_PAR_BOTTOM_HALF,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_PAR_BOTTOM_QUARTER,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_PAR_BOTTOM_EIGHTH,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_PAR_TOP_HALF = 5,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_PAR_TOP_QUARTER,
    AM_DEVICES_XSPI_PSRAM_W958D6NW_PAR_TOP_EIGHTH,
} am_devices_xspi_psram_w958d6nw_par_e;

typedef struct
{
    union
    {
        uint16_t ID0;                               // Identification register 0

        struct
        {
            uint16_t MANUFACTURER   : 4;            // [3:0] 0110b = Winbond
            uint16_t COL_BITS       : 4;            // [7:4] 0111b = 8 column bits
            uint16_t ROW_BITS       : 5;            // [12:8] 01110b = 15 row bits
            uint16_t                : 3;
        } ID0_b;
    };

    union
    {
        uint16_t ID1;                               // Identification register 1

        struct
        {
            uint16_t DEVICE_TYPE    : 4;            // [3:0] 1001b = HyperRAM-Extend-IO
            uint16_t                : 12;
        } ID1_b;
    };

    union
    {
        uint16_t CR0;                               // Configuration register 0

        struct
        {
            uint16_t BL             : 2;            // [1:0] Burst length
            uint16_t HYBRID_BURST   : 1;            // [2] 0=hybrid, 1=legacy wrapped
            uint16_t FIXED_LATENCY  : 1;            // [3] 0=variable, 1=fixed
            uint16_t ILC            : 4;            // [7:4] Initial latency code
            uint16_t                : 4;
            uint16_t DS             : 3;            // [14:12] Drive strength
            uint16_t DPD            : 1;            // [15] 1=normal, 0=enter DPD
        } CR0_b;
    };

    union
    {
        uint16_t CR1;                               // Configuration register 1

        struct
        {
            uint16_t REFRESH        : 2;            // [1:0] Distributed refresh interval
            uint16_t PAR            : 3;            // [4:2] Partial array refresh
            uint16_t HYBRID_SLEEP   : 1;            // [5] Write 1 to enter Hybrid Sleep
            uint16_t CLOCK_TYPE     : 1;            // [6] 1=single-ended, 0=differential
            uint16_t                : 1;
            uint16_t                : 8;
        } CR1_b;
    };
} am_devices_xspi_psram_w958d6nw_regs_t;

typedef struct
{
    /* data */
} am_devices_xspi_psram_w958d6nw_power_cfg_t;

//! @}

//*****************************************************************************
//
//! @name External function definitions
//! @{
//
//*****************************************************************************
extern const am_device_xspi_func_t apiW958D6NW;

//! @}

//*****************************************************************************
//
//! @name Common peripheral Helper Macro
//! @{
//
//*****************************************************************************
#define AM_DEVICE_XSPI_PSRAM_W958D6NW_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM, XSPI_HANDLE) \
        AM_DEVICE_XSPI_DEV_HANDLE_DEFINE(ATTR __ALIGNED(4), MODULE_NAME, CE_NUM,                         \
                                         AM_DEVICES_XSPI_PSRAM_W958D6NW_GLOBAL_RAM_SIZE,    \
                                         apiW958D6NW, XSPI_HANDLE)

//! @}

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_PSRAM_W958D6NW_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
