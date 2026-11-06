//*****************************************************************************
//
//! @file am_devices_xspi_psram_aps_z8.h
//!
//! @brief Multi-bit SPI PSRAM driver for the APS Z8 device.
//!
//! @addtogroup devices_xspi_psram_aps_z8 APS25616N/APS25616B xSPI PSRAM Driver
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

#ifndef AM_DEVICES_XSPI_PSRAM_APS_Z8_H
#define AM_DEVICES_XSPI_PSRAM_APS_Z8_H

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
#define AM_DEVICES_XSPI_PSRAM_APS_Z8_GLOBAL_RAM_SIZE  132

//! @}

//*****************************************************************************
//
//! @name Global definitions for psram commands
//! @{
//
//*****************************************************************************
#define AM_DEVICES_XSPI_PSRAM_APS_Z8_GLOBAL_RESET      0xFF
#define AM_DEVICES_XSPI_PSRAM_APS_Z8_READ              0x20    //linear burst read
#define AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE             0xA0    //linear burst write
#define AM_DEVICES_XSPI_PSRAM_APS_Z8_READ_REGISTER     0x40
#define AM_DEVICES_XSPI_PSRAM_APS_Z8_WRITE_REGISTER    0xC0

#define AM_DEVICES_XSPI_PSRAM_APS_Z8_REG_ADDR_LEN      0x4

//! @}

//! @{
//
//! @note The following definitions are typically specific to a multibit spi psram device.
//! They should be tailored
//
//*****************************************************************************
//
//! @name Device specific identification.
//! @{
//
//*****************************************************************************
// Page size - limits the bust write/read
#define AM_DEVICES_XSPI_PSRAM_APS_Z8_PAGE_SIZE         1024

//! @}

//! @}

//*****************************************************************************
//
// Global type definitions.
//
//*****************************************************************************
typedef enum
{
#if !defined(USE_APS51216BA)
    AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_3,
#endif
    AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_4,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_5,  //device power up default for APS25616BA
    AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_6,  //device power up default for APS51216BA
    AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_7,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_8,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_RLC_9,
} am_devices_xspi_psram_aps_z8_rlc_e;

typedef enum
{
    AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_3, //reserved for APS51216BA
    AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_7,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_5,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_9,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_4, //reserved for APS51216BA
    AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_8,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_6, //device power up default
    AM_DEVICES_XSPI_PSRAM_APS_Z8_WLC_10,
} am_devices_xspi_psram_aps_z8_wlc_e;

typedef enum
{
    AM_DEVICES_XSPI_PSRAM_APS_Z8_DRIVE_STRENGTH_FULL,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_DRIVE_STRENGTH_HALF,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_DRIVE_STRENGTH_QUARTER,
    AM_DEVICES_XSPI_PSRAM_APS_Z8_DRIVE_STRENGTH_OCTUPLE,
} am_devices_xspi_psram_aps_z8_ds_e;

typedef struct
{
    union
    {
        uint8_t MR0;                                //Mode register 0

        struct
        {
            uint8_t DS              : 2;            //[0..1] drive strength                |       00=Full(25 ohm), 01=Half(50 ohm) default,                    |       volatile
                                                    //                                     |       10=Quarter(100 ohm), 11=Octuple (200 ohm)                    |
            uint8_t RLC             : 3;            //[2..4] Read latency code.            |       000 = LC:3, LCx2:6,  33MHz                                   |       volatile
                                                    //                                     |       001 = LC:4, LCx2:8,  66MHz                                   |
                                                    //                                     |       010 = LC:5, LCx2:10, 109MHz (default)                        |
                                                    //                                     |       011 = LC:6, LCx2:12, 133MHz                                  |
                                                    //                                     |       100 = LC:7, LCx2:14, 166MHz                                  |
                                                    //                                     |       101 = LC:8, LCx2:16, 200MHz                                  |
                                                    //                                     |       110 = LC:9, LCx2:18, 200MHz                                  |
            uint8_t LT              : 1;            //[5..5] Latency type                  |       0=Variable(default), 1=Fixed                                 |       volatile
            uint8_t                 : 1;
            uint8_t TSO             : 1;            //[7..7] Temperature Sensor Override   |       0=self control refresh, 1=force refresh rate set by MR4.RFR  |       volatile
        }MR0_b;
    };

    union
    {
        uint8_t MR1;                                //Mode register 1

        struct
        {
            uint8_t VID             : 5;            //[0..4] Vendor ID                     |       5'b01101                                                     |       non-volatile
            uint8_t                 : 2;
            uint8_t ULP             : 1;            //[7..7] Half sleep.                   |       0=no half sleep, 1=half sleep supported                      |       non-volatile
        }MR1_b;
    };

    union
    {
        uint8_t MR2;                                //Mode register 2

        struct
        {
            uint8_t DENSITY         : 3;            //[0..2] Density                       |                                                                    |       non-volatile
            uint8_t GENERATION      : 2;            //[3..4] Generation                    |
            uint8_t GBD             : 3;            //[5..7] Good or bad die               |      others=bad, 3'b110=pass                                       |       non-volatile
        }MR2_b;
    };

    union
    {
        uint8_t MR3;                                //Mode register 3

        struct
        {
            uint8_t                 : 4;
            uint8_t SRF             : 2;            //[4..5] Self refresh flag             |      00=0.5x refresh, 01=1x refresh, 10=4x refresh                 |       non-volatile
            uint8_t                 : 1;
            uint8_t RBXen           : 1;            //[7..7] Row Boundary Crossing Enable  |      0=not supported, 1=supported                                  |       non-volatile
        }MR3_b;
    };

    union
    {
        uint8_t MR4;                                //Mode register 4

        struct
        {
            uint8_t PASR            : 3;            //[0..2] control refresh address space |      0~7=different address space. default 0=full array             |       volatile
            uint8_t RFS             : 2;            //[3..4] Refresh Frequency setting     |      x0=always 4x(default),                                        |       volatile
                                                    //                                     |      1=enable 1x when temperature allows,
                                                    //                                     |      3=enable 0.5x when temperature allows,
            uint8_t WLC             : 3;            //[5..7] Write latency code.           |      000 = LC:3,   33MHz                                           |       volatile
                                                    //                                     |      100 = LC:4,   33MHz
                                                    //                                     |      010 = LC:5,   66MHz
                                                    //                                     |      110 = LC:6,  109MHz (default)
                                                    //                                     |      001 = LC:7,  133MHz
                                                    //                                     |      101 = LC:8,  166MHz
                                                    //                                     |      011 = LC:9,  200MHz
                                                    //                                     |      111 = LC:10, 200MHz
        }MR4_b;
    };

    union
    {
        uint8_t MR6;                                //Mode register 6

        struct
        {
            uint8_t ULPM            : 8;            //[0..7] ULP modes                     |      0xF0=Half slepp, 0xC0=Deep power down                         |       volatile
        }MR6_b;
    };

    union
    {
        uint8_t MR8;                                //Mode register 8

        struct
        {
            uint8_t BL              : 2;            //[0..1] Burst Length                  |      0~4=16B,32B,64B,2KB wrap                                      |       volatile
            uint8_t BT              : 1;            //[2..2] Burst type                    |      0=normal, 1=hybrid
            uint8_t RBX             : 1;            //[3..3] Row Boundary Crossing Read EN |      0=Reads stay within 1K, 1=Reads cross 1K boundray             |       volatile
            uint8_t                 : 2;
            uint8_t IOM             : 1;            //[6..6] IO mode                       |      0=octal default, 1=hex
            uint8_t                 : 1;
        }MR8_b;
    };
} am_devices_xspi_psram_aps_z8_regs_t;

typedef struct
{
    /* data */
} am_devices_xspi_psram_aps_z8_power_cfg_t;


//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************
extern const am_device_xspi_func_t apiAPSZ8;


//*****************************************************************************
//
//! @brief Common peripheral Helper Macro.
//! @defgroup devices_xspi_psram_aps_z8_macros
//! @ingroup devices_xspi_psram_aps_z8
//! @{
//
//*****************************************************************************
#define AM_DEVICE_XSPI_PSRAM_APS_Z8_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM, XSPI_HANDLE) \
        AM_DEVICE_XSPI_DEV_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM,                       \
                                         AM_DEVICES_XSPI_PSRAM_APS_Z8_GLOBAL_RAM_SIZE,    \
                                         apiAPSZ8, XSPI_HANDLE)

//! @} END_GROUP - Common peripheral Helper Macro.
#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_PSRAM_APS_Z8_H

//*****************************************************************************
// End Doxygen group.
//! @}
//
//*****************************************************************************
